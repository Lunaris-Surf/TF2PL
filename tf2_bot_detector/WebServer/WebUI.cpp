// cpp-httplib's CPPHTTPLIB_*_SUPPORT macros come from the httplib::httplib CMake target.
#pragma warning(push, 1)
#include <httplib.h>
#pragma warning(pop)

#include "WebUI.h"
#include "WebUIData.h"
#include "HostAlias.h"
#include "Application.h"
#include "Filesystem.h"
#include "Log.h"
#include "ModeratorLogic.h"
#include "PlayerListExport.h"
#include "PreviewMode.h"
#include "StreamerMode.h"
#include "UpdateManager.h"
#include "WorldState.h"
#include "Actions/Actions.h"
#include "Config/PlayerListJSON.h"
#include "Config/Settings.h"
#include "GameData/IPlayer.h"
#include "Platform/Platform.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <random>

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace tf2_bot_detector;

namespace
{
	// index.html, embedded at build time by WebServer/EmbedFile.cmake (split into chunks for MSVC's literal limit)
	constexpr const char* WEB_INDEX_CHUNKS[] =
	{
#include "WebUIAssets.inl"
	};

	constexpr std::string_view TOKEN_PLACEHOLDER = "__TF2PL_TOKEN__";
	constexpr size_t MAX_LIVE_CLIENTS = 4;

	std::string LoadIndexTemplate()
	{
		std::string html;
		for (const char* chunk : WEB_INDEX_CHUNKS)
			html += chunk;
		return html;
	}

	std::string GenerateToken()
	{
		std::random_device rd;
		std::uniform_int_distribution<int> dist(0, 15);
		std::string token;
		token.reserve(40);
		for (int i = 0; i < 40; i++)
			token += "0123456789abcdef"[dist(rd)];
		return token;
	}

	bool ConstantTimeEquals(std::string_view a, std::string_view b)
	{
		if (a.size() != b.size())
			return false;
		unsigned char diff = 0;
		for (size_t i = 0; i < a.size(); i++)
			diff |= static_cast<unsigned char>(a[i] ^ b[i]);
		return diff == 0;
	}

	std::optional<SteamID> SteamIDFromJson(const nlohmann::json& action)
	{
		auto found = action.find("steamid");
		if (found == action.end() || !found->is_string())
			return std::nullopt;

		try
		{
			SteamID id(uint64_t(std::stoull(found->get<std::string>())));
			if (id.IsValid())
				return id;
		}
		catch (...)
		{
		}
		return std::nullopt;
	}

	std::string StringField(const nlohmann::json& action, const char* name, size_t maxLength = 512)
	{
		auto found = action.find(name);
		if (found == action.end() || !found->is_string())
			return {};

		std::string value = found->get<std::string>();
		if (value.size() > maxLength)
			value.resize(maxLength);
		return value;
	}

	bool BoolField(const nlohmann::json& action, const char* name)
	{
		auto found = action.find(name);
		if (found == action.end() || !found->is_boolean())
			throw std::invalid_argument(fmt::format("missing true/false field \"{}\"", name));
		return found->get<bool>();
	}

	const char* ImageContentType(const std::string& path)
	{
		std::string ext = std::filesystem::path(std::u8string(path.begin(), path.end())).extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
		if (ext == ".png") return "image/png";
		if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
		if (ext == ".gif") return "image/gif";
		if (ext == ".bmp") return "image/bmp";
		return nullptr; // tga etc. can't be shown by browsers
	}
}

const char* WebUIController::ChannelName(size_t channel)
{
	switch (channel)
	{
	case Channel_State:    return "state";
	case Channel_Chat:     return "chat";
	case Channel_Log:      return "log";
	case Channel_Settings: return "settings";
	case Channel_Dossier:  return "dossier";
	case Channel_Lists:    return "lists";
	default:               return "unknown";
	}
}

WebUIController::WebUIController(TF2BDApplication& app) :
	m_Application(app)
{
}

WebUIController::~WebUIController()
{
	Stop();
}

bool WebUIController::IsRunning() const
{
	return m_Server && m_ThreadExited && !*m_ThreadExited;
}

std::string WebUIController::GetURL() const
{
	const auto port = m_Port ? m_Port : m_Application.GetSettings().m_WebUIPort;
	const char* host = m_HostAliasReady ? WebHostAlias::NAME : "127.0.0.1";
	return port == 80 ? fmt::format("http://{}/", host) : fmt::format("http://{}:{}/", host, port);
}

std::string WebUIController::GetError() const
{
	std::lock_guard lock(m_Mutex);
	return m_Error;
}

void WebUIController::OpenInBrowser() const
{
	Platform::Shell::OpenURL(GetURL());
}

void WebUIController::Publish(Channel channel, std::string data)
{
	{
		std::lock_guard lock(m_Mutex);
		auto& published = m_Channels[channel];
		if (published.m_Data == data)
			return;
		published.m_Data = std::move(data);
		published.m_Version++;
	}
	m_Changed.notify_all();
}

bool WebUIController::Start(uint16_t port)
{
	Stop();

	m_Token = GenerateToken();
	const std::string index = [&]
	{
		std::string html = LoadIndexTemplate();
		if (auto pos = html.find(TOKEN_PLACEHOLDER); pos != html.npos)
			html.replace(pos, TOKEN_PLACEHOLDER.size(), m_Token);
		return html;
	}();

	auto server = std::make_unique<httplib::Server>();
	server->set_payload_max_length(64 * 1024);
	server->set_read_timeout(5, 0);
	server->set_write_timeout(5, 0);

	const std::string token = m_Token;

	server->set_pre_routing_handler([port](const httplib::Request& req, httplib::Response& res)
		{
			// DNS rebinding: a page on evil.example that resolves to 127.0.0.1 still sends Host: evil.example
			const auto host = req.get_header_value("Host");
			if (!WebHostAlias::IsAllowedHost(host, port))
			{
				res.status = 403;
				res.set_content("Forbidden", "text/plain");
				return httplib::Server::HandlerResponse::Handled;
			}
			return httplib::Server::HandlerResponse::Unhandled;
		});

	server->set_post_routing_handler([](const httplib::Request&, httplib::Response& res)
		{
			res.set_header("Cache-Control", "no-store");
			res.set_header("X-Content-Type-Options", "nosniff");
			res.set_header("X-Frame-Options", "DENY");
			res.set_header("Referrer-Policy", "no-referrer");
		});

	// fetch() sends the token as a header; EventSource and <img> can't, so they put it in the query string
	const auto Authorized = [token](const httplib::Request& req, httplib::Response& res, bool allowQuery = false)
		{
			if (ConstantTimeEquals(req.get_header_value("X-TF2PL-Token"), token) ||
				(allowQuery && ConstantTimeEquals(req.get_param_value("token"), token)))
			{
				return true;
			}
			res.status = 401;
			res.set_content(R"({"error":"unauthorized"})", "application/json");
			return false;
		};

	server->Get("/", [index](const httplib::Request&, httplib::Response& res)
		{
			res.set_header("Content-Security-Policy",
				"default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; "
				"img-src 'self' data: https://steamcdn-a.akamaihd.net https://*.steamstatic.com; "
				"base-uri 'none'; form-action 'none'; frame-ancestors 'none'");
			res.set_content(index, "text/html; charset=utf-8");
		});

	// everything at once (polling fallback, and the first paint)
	server->Get("/api/state", [this, Authorized](const httplib::Request& req, httplib::Response& res)
		{
			if (!Authorized(req, res))
				return;

			std::string body = "{";
			{
				std::lock_guard lock(m_Mutex);
				for (size_t i = 0; i < Channel_Count; i++)
				{
					if (i)
						body += ',';
					body += '"';
					body += ChannelName(i);
					body += "\":";
					body += m_Channels[i].m_Data;
				}
			}
			body += '}';
			res.set_content(body, "application/json");
		});

	// live updates: one SSE event per channel whenever it changes
	server->Get("/api/events", [this, Authorized](const httplib::Request& req, httplib::Response& res)
		{
			if (!Authorized(req, res, true))
				return;

			if (m_LiveClients >= MAX_LIVE_CLIENTS)
			{
				res.status = 503; // the page falls back to polling
				return;
			}
			m_LiveClients++;

			auto sent = std::make_shared<std::array<uint64_t, Channel_Count>>();
			sent->fill(0);

			res.set_header("X-Accel-Buffering", "no");
			res.set_chunked_content_provider("text/event-stream",
				[this, sent](size_t, httplib::DataSink& sink)
				{
					std::string out;
					{
						std::unique_lock lock(m_Mutex);
						const auto HasNews = [&]
						{
							if (m_Stopping)
								return true;
							for (size_t i = 0; i < Channel_Count; i++)
							{
								if (m_Channels[i].m_Version != (*sent)[i])
									return true;
							}
							return false;
						};

						if (!m_Changed.wait_for(lock, 15s, HasNews))
						{
							out = ": keepalive\n\n";
						}
						else
						{
							if (m_Stopping)
								return false;

							for (size_t i = 0; i < Channel_Count; i++)
							{
								if (m_Channels[i].m_Version == (*sent)[i])
									continue;
								(*sent)[i] = m_Channels[i].m_Version;
								out += "event: ";
								out += ChannelName(i);
								out += "\ndata: ";
								out += m_Channels[i].m_Data; // compact JSON, never contains newlines
								out += "\n\n";
							}
						}
					}

					return sink.is_writable() && sink.write(out.data(), out.size());
				},
				[this](bool)
				{
					m_LiveClients--;
				});
		});

	// streamer mode's custom avatar (the page never learns the file path)
	server->Get("/api/avatar", [this, Authorized](const httplib::Request& req, httplib::Response& res)
		{
			if (!Authorized(req, res, true))
				return;

			std::string path;
			{
				std::lock_guard lock(m_Mutex);
				path = m_StreamerAvatarPath;
			}

			const char* type = path.empty() ? nullptr : ImageContentType(path);
			if (!type)
			{
				res.status = 404;
				return;
			}

			std::ifstream file(std::filesystem::path(std::u8string(path.begin(), path.end())), std::ios::binary);
			if (!file)
			{
				res.status = 404;
				return;
			}

			std::string data;
			data.resize(8 * 1024 * 1024 + 1);
			file.read(data.data(), std::streamsize(data.size()));
			const auto read = size_t(file.gcount());
			if (read > 8 * 1024 * 1024)
			{
				res.status = 413;
				return;
			}
			data.resize(read);
			res.set_content(data, type);
		});

	server->Post("/api/action", [this, Authorized](const httplib::Request& req, httplib::Response& res)
		{
			if (!Authorized(req, res))
				return;
			if (req.get_header_value("Content-Type").find("application/json") != 0)
			{
				res.status = 415;
				return;
			}

			{
				std::lock_guard lock(m_Mutex);
				if (m_Actions.size() >= 64)
				{
					res.status = 429;
					res.set_content(R"({"error":"too many queued actions"})", "application/json");
					return;
				}
				m_Actions.push_back(req.body);
			}
			res.status = 202;
			res.set_content(R"({"queued":true})", "application/json");
		});

	if (!server->bind_to_port("127.0.0.1", port))
	{
		std::lock_guard lock(m_Mutex);
		m_Error = fmt::format("Couldn't listen on 127.0.0.1:{} - is another program (or another TF2PL) using that port?", port);
		LogWarning("Web UI: {}", m_Error);
		return false;
	}

	{
		std::lock_guard lock(m_Mutex);
		m_Error.clear();
		m_Stopping = false;
	}

	m_Port = port;
	m_Server = std::move(server);
	m_ThreadExited = std::make_shared<std::atomic_bool>(false);
	m_Thread = std::thread([server = m_Server.get(), exited = m_ThreadExited]()
		{
			server->listen_after_bind();
			*exited = true;
		});

	m_ForceRefresh = true;
	if (!m_HostAliasAttempted)
	{
		m_HostAliasAttempted = true;
		// Authentication and the helper run off the update thread. At most one
		// prompt per controller lifetime, including server restarts/port changes.
		m_HostAliasSetup = std::async(std::launch::async, WebHostAlias::EnsureInstalled);
	}
	Log("Web UI listening on {}", GetURL());
	return true;
}

void WebUIController::Stop()
{
	{
		std::lock_guard lock(m_Mutex);
		m_Stopping = true; // wakes up and ends the live (SSE) streams
	}
	m_Changed.notify_all();

	if (m_Server)
		m_Server->stop();
	if (m_Thread.joinable())
		m_Thread.join();
	m_Server.reset();
	m_ThreadExited.reset();
	m_Port = 0;
}

void WebUIController::Update()
{
	auto& settings = m_Application.GetSettings();
	const auto now = tfbd_clock_t::now();

	const bool portChanged = m_Server && m_Port != settings.m_WebUIPort;
	if (portChanged || (!IsRunning() && now - m_LastStartAttempt > 5s))
	{
		m_LastStartAttempt = now;
		Start(settings.m_WebUIPort);
	}

	if (!IsRunning())
		return;

	if (m_HostAliasSetup.valid() && m_HostAliasSetup.wait_for(0s) == std::future_status::ready)
	{
		m_HostAliasReady = m_HostAliasSetup.get();
		Log("Web UI available at {}", GetURL());
	}
	if (!m_HostAliasSetup.valid() && settings.m_WebUIOpenBrowser && !m_BrowserOpened)
	{
		OpenInBrowser();
		m_BrowserOpened = true;
	}

	// the page's changes come back in the very next push
	RunQueuedActions();
	Refresh(false);
}

void WebUIController::Refresh(bool force)
{
	force = force || m_ForceRefresh;
	m_ForceRefresh = false;

	const auto now = tfbd_clock_t::now();
	auto& settings = m_Application.GetSettings();

	{
		std::lock_guard lock(m_Mutex);
		m_StreamerAvatarPath = settings.m_StreamerMode.m_Enabled ? settings.m_StreamerMode.m_AvatarPath : std::string{};
	}

	const auto Dump = [](const nlohmann::json& j)
	{
		// invalid UTF-8 in player names must not take the whole snapshot down
		return j.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
	};

	const auto Build = [&](Channel channel, const auto& builder)
	{
		try
		{
			Publish(channel, Dump(builder()));
		}
		catch (const std::exception& e)
		{
			LogException(TF2PL_SOURCE_LOCATION_CURRENT(), e, "Web UI: failed to build {}", ChannelName(channel));
		}
	};

	if (force || now - m_LastFastRefresh >= 100ms)
	{
		m_LastFastRefresh = now;

		Build(Channel_State, [&] { return WebUIData::BuildState(m_Application, m_Exporter.get()); });
		Build(Channel_Chat, [&] { return WebUIData::BuildChat(m_Application); });
		Build(Channel_Log, [&] { return WebUIData::BuildLog(); });

		Build(Channel_Dossier, [&]() -> nlohmann::json
			{
				if (m_FocusedSelf)
				{
					const SteamID self = settings.GetLocalSteamID();
					return self.IsValid() ? WebUIData::BuildDossier(m_Application, self) : nlohmann::json();
				}
				return m_FocusedPlayer ? WebUIData::BuildDossier(m_Application, *m_FocusedPlayer) : nlohmann::json();
			});

		if (m_ListQuery)
		{
			const auto* playerList = m_Application.GetModLogic().GetPlayerList();
			const size_t count = playerList ? playerList->GetPlayerCount() : 0;
			if (m_ListsDirty || count != m_ListQueryPlayerCount)
			{
				m_ListsDirty = false;
				m_ListQueryPlayerCount = count;
				Build(Channel_Lists, [&] { return WebUIData::BuildLists(m_Application, WebUIData::ParseListQuery(*m_ListQuery)); });
			}
		}
	}

	if (force || now - m_LastSlowRefresh >= 1s)
	{
		m_LastSlowRefresh = now;
		Build(Channel_Settings, [&] { return WebUIData::BuildSettings(m_Application); });
	}
}

void WebUIController::RunQueuedActions()
{
	std::deque<std::string> actions;
	{
		std::lock_guard lock(m_Mutex);
		actions.swap(m_Actions);
	}

	for (const auto& body : actions)
	{
		try
		{
			const auto action = nlohmann::json::parse(body);
			if (!action.is_object())
				throw std::invalid_argument("action must be a JSON object");
			RunAction(action);
		}
		catch (const std::exception& e)
		{
			LogWarning("Web UI: rejected action: {}", e.what());
		}
	}

	if (!actions.empty())
		m_ForceRefresh = true; // send the results right away
}

void WebUIController::RunAction(const nlohmann::json& action)
{
	auto& settings = m_Application.GetSettings();
	const std::string type = StringField(action, "type", 32);

	// --- app-wide -----------------------------------------------------------------------------------------------
	if (type == "ui_mode")
	{
		if (StringField(action, "mode", 16) == "native")
		{
			settings.m_UIMode = TF2BDUIMode::Native;
			settings.SaveFile();
			Log("Web UI: switched back to the native UI");
		}
		return;
	}
	if (type == "setting")
	{
		const auto found = action.find("value");
		if (found == action.end())
			throw std::invalid_argument("missing value");
		WebUIData::ApplySetting(m_Application, StringField(action, "id", 128), *found);
		return;
	}
	if (type == "toggle")
	{
		const std::string name = StringField(action, "name", 32);
		const bool value = BoolField(action, "value");
		if (name == "pause")
			m_Application.SetPaused(value);
		else if (name == "show_commands")
			settings.m_Unsaved.m_DebugShowCommands = value;
		else if (name == "preview")
			m_Application.SetPreviewMode(value);
		else
		{
			// the rest are saved settings
			static const std::pair<const char*, const char*> SAVED[] =
			{
				{ "chat_warnings", "auto_chat_warnings" }, { "party_warnings", "party_warnings" },
				{ "auto_votekick", "auto_votekick" }, { "auto_mark", "auto_mark" }, { "auto_mark_bans", "auto_mark_bans" },
				{ "streamer", "streamer.enabled" }, { "kill_logs", "kill_logs" },
				{ "view.chat", "view.chat" }, { "view.scoreboard", "view.scoreboard" },
				{ "view.team_stats", "view.team_stats" }, { "view.app_log", "view.app_log" },
			};
			const auto found = std::find_if(std::begin(SAVED), std::end(SAVED), [&](const auto& p) { return name == p.first; });
			if (found == std::end(SAVED))
				throw std::invalid_argument("unknown toggle");
			WebUIData::ApplySetting(m_Application, found->second, value);
		}
		return;
	}
	if (type == "reload_lists")
	{
		m_Application.GetModLogic().ReloadConfigFiles();
		m_ListsDirty = true;
		return;
	}
	if (type == "reload_settings")
	{
		settings.LoadFile();
		return;
	}
	if (type == "open_folder")
	{
		const std::string which = StringField(action, "which", 16);
		if (which == "config")
			Platform::Shell::ExploreTo(IFilesystem::Get().GetConfigDir());
		else if (which == "logs")
			Platform::Shell::ExploreTo(IFilesystem::Get().GetLogsDir());
		else
			throw std::invalid_argument("unknown folder");
		return;
	}
	if (type == "debug_report")
	{
		GenerateDebugReport();
		return;
	}
	if (type == "update_check")
	{
		if (auto* updates = m_Application.GetUpdateManager())
			updates->QueueUpdateCheck();
		return;
	}
	if (type == "browse_avatar")
	{
		// opens the file picker on this PC (the page is on the same machine)
		if (auto path = Platform::Shell::BrowseForImageDialog(); !path.empty())
		{
			const auto u8 = path.u8string();
			WebUIData::ApplySetting(m_Application, "streamer.avatar", std::string(u8.begin(), u8.end()));
		}
		return;
	}
	if (type == "untrusted_dismiss")
	{
		m_Application.DismissUntrustedListReport(action.value("acknowledge", false));
		return;
	}
	if (type == "export")
	{
		const std::string op = StringField(action, "op", 16);
		if (op == "start")
		{
			if (m_Exporter && m_Exporter->IsRunning())
				return;
			auto* playerList = m_Application.GetModLogic().GetPlayerList();
			if (!playerList)
				throw std::invalid_argument("player lists aren't loaded");
			m_Exporter = std::make_unique<PlayerListExporter>(settings, *playerList);
			m_Exporter->Start();
		}
		else if (op == "cancel")
		{
			if (m_Exporter)
				m_Exporter->Cancel();
		}
		else if (op == "close")
		{
			if (m_Exporter && !m_Exporter->IsRunning())
				m_Exporter.reset();
		}
		else
		{
			throw std::invalid_argument("unknown export op");
		}
		return;
	}

	// --- player lists browser -------------------------------------------------------------------------------------
	if (type == "lists_query")
	{
		m_ListQuery = action;
		m_ListsDirty = true;
		return;
	}
	if (type == "lists_close")
	{
		m_ListQuery.reset();
		Publish(Channel_Lists, "null");
		return;
	}

	// --- one player -----------------------------------------------------------------------------------------------
	if (type == "focus")
	{
		m_FocusedSelf = StringField(action, "steamid", 32) == "self";
		m_FocusedPlayer = m_FocusedSelf ? std::nullopt : SteamIDFromJson(action);
		return;
	}
	if (type == "lookup")
	{
		const auto id = WebUIData::ParseSteamID(StringField(action, "text", 256));
		if (!id)
			throw std::invalid_argument("not a valid SteamID");
		m_FocusedSelf = false;
		m_FocusedPlayer = id;
		return;
	}

	const auto id = SteamIDFromJson(action);
	if (!id)
		throw std::invalid_argument("missing or invalid steamid");
	if (*id == settings.GetLocalSteamID())
		throw std::invalid_argument("can't act on the local player");

	auto& world = m_Application.GetWorld();
	auto& modLogic = m_Application.GetModLogic();
	const IPlayer* player = world.FindPlayer(*id);
	std::string name = player ? player->GetNameUnsafe() : std::string{};
	if (name.empty())
	{
		if (auto* playerList = modLogic.GetPlayerList())
		{
			for (const auto& [fileName, data] : playerList->FindPlayerData(*id))
			{
				if (data.m_LastSeen && !data.m_LastSeen->m_PlayerName.empty())
				{
					name = data.m_LastSeen->m_PlayerName;
					break;
				}
			}
		}
	}
	if (name.empty())
		name = StringField(action, "name", 128); // "Add player" in Player Lists can supply one

	const std::string reason = StringField(action, "reason");
	const bool set = action.value("set", true);

	if (type == "mark")
	{
		const auto attr = action.at("attr").get<PlayerAttribute>(); // throws on unknown ids
		if (modLogic.SetPlayerAttribute(*id, name, attr, AttributePersistence::Saved, set, reason))
			Log("Web UI: {} {} {:v} | {}", name.empty() ? id->str() : name, set ? "marked" : "unmarked", tf2pl::enum_fmt(attr), reason);
		m_ListsDirty = true;
	}
	else if (type == "tag")
	{
		const std::string tag = StringField(action, "tag", 80);
		if (!PlayerAttributesList::IsValidCustomTag(tag))
			throw std::invalid_argument("invalid custom tag");
		modLogic.SetPlayerCustomTag(*id, name, tag, AttributePersistence::Saved, set, reason);
		m_ListsDirty = true;
	}
	else if (type == "remove_from_list")
	{
		auto* preview = m_Application.GetPreview();
		if (preview && preview->OwnsPlayer(*id))
			throw std::invalid_argument("sample players aren't in your list");
		if (auto* playerList = modLogic.GetPlayerList(); playerList &&
			playerList->RemoveFromWritableList(*id) == ModifyPlayerResult::FileSaved)
		{
			Log("Web UI: removed {} from {}", name.empty() ? id->str() : name, playerList->GetWritableListName());
		}
		m_ListsDirty = true;
	}
	else if (type == "votekick")
	{
		if (!player)
			throw std::invalid_argument("player is not in the game");

		const std::string kickReason = StringField(action, "reason", 16);
		KickReason reasonValue = KickReason::Cheating;
		if (kickReason == "idle")
			reasonValue = KickReason::Idle;
		else if (kickReason == "other")
			reasonValue = KickReason::Other;
		else if (kickReason == "scamming")
			reasonValue = KickReason::Scamming;
		else if (kickReason != "cheating")
			throw std::invalid_argument("unknown votekick reason");

		modLogic.InitiateVotekick(*player, reasonValue);
	}
	else
	{
		throw std::invalid_argument("unknown action type");
	}
}
