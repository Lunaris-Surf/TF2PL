// cpp-httplib's CPPHTTPLIB_*_SUPPORT macros come from the httplib::httplib CMake target.
#pragma warning(push, 1)
#include <httplib.h>
#pragma warning(pop)

#include "WebUI.h"
#include "Application.h"
#include "Log.h"
#include "ModeratorLogic.h"
#include "PlayerStatus.h"
#include "WorldState.h"
#include "Actions/Actions.h"
#include "Config/PlayerListJSON.h"
#include "Config/Settings.h"
#include "ConsoleLog/IConsoleLine.h"
#include "ConsoleLog/ConsoleLines/ChatConsoleLine.h"
#include "GameData/IPlayer.h"
#include "GameData/TFConstants.h"
#include "Networking/SteamAPI.h"
#include "Platform/Platform.h"
#include "Version.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <atomic>
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

	const char* TeamName(TFTeam team)
	{
		switch (team)
		{
		case TFTeam::Red:       return "red";
		case TFTeam::Blue:      return "blue";
		case TFTeam::Spectator: return "spectator";
		default:                return "unknown";
		}
	}

	const char* StateName(PlayerStatusState state)
	{
		switch (state)
		{
		case PlayerStatusState::Challenging: return "challenging";
		case PlayerStatusState::Connecting:  return "connecting";
		case PlayerStatusState::Spawning:    return "spawning";
		case PlayerStatusState::Active:      return "active";
		default:                             return "invalid";
		}
	}

	const char* TeamShareName(TeamShareResult result)
	{
		switch (result)
		{
		case TeamShareResult::SameTeams:     return "same";
		case TeamShareResult::OppositeTeams: return "opposite";
		default:                             return "neither";
		}
	}

	std::string ColorHex(const std::array<float, 4>& c)
	{
		const auto ch = [](float v) { return int(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
		return fmt::format("#{:02x}{:02x}{:02x}", ch(c[0]), ch(c[1]), ch(c[2]));
	}

	const std::array<float, 4>* AttributeColor(const Settings::Theme::Colors& colors, PlayerAttribute attr)
	{
		switch (attr)
		{
		case PlayerAttribute::Cheater:           return &colors.m_ScoreboardCheaterBG;
		case PlayerAttribute::Suspicious:        return &colors.m_ScoreboardSuspiciousBG;
		case PlayerAttribute::Exploiter:         return &colors.m_ScoreboardExploiterBG;
		case PlayerAttribute::Racist:            return &colors.m_ScoreboardRacistBG;
		case PlayerAttribute::Hostile:           return &colors.m_ScoreboardHostileBG;
		case PlayerAttribute::SuspectedCheater:  return &colors.m_ScoreboardSuspectedCheaterBG;
		case PlayerAttribute::Blacklisted:       return &colors.m_ScoreboardBlacklistedBG;
		case PlayerAttribute::VACBanned:         return &colors.m_ScoreboardVACBannedBG;
		case PlayerAttribute::GameBanned:        return &colors.m_ScoreboardGameBannedBG;
		case PlayerAttribute::SourceBanned:      return &colors.m_ScoreboardSourceBannedBG;
		case PlayerAttribute::Pedophilia:        return &colors.m_ScoreboardPedophiliaBG;
		case PlayerAttribute::UntrustedListUser: return &colors.m_ScoreboardUntrustedListUserBG;
		case PlayerAttribute::Zoophilia:         return &colors.m_ScoreboardZoophiliaBG;
		default:                                 return nullptr;
		}
	}

	nlohmann::json AttributesToJson(const PlayerAttributesList& attrs)
	{
		nlohmann::json out = nlohmann::json::array();
		for (size_t i = 0; i < PlayerAttributesList::size(); i++)
		{
			if (attrs.HasAttribute(PlayerAttribute(i)))
				out.push_back(nlohmann::json(PlayerAttribute(i)));
		}
		for (const auto& tag : attrs.GetCustomTags())
			out.push_back(tag);
		return out;
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
	return fmt::format("http://127.0.0.1:{}/", m_Port ? m_Port : m_Application.GetSettings().m_WebUIPort);
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
	server->set_payload_max_length(16 * 1024);
	server->set_read_timeout(5, 0);
	server->set_write_timeout(5, 0);

	const std::string allowedHosts[] = { fmt::format("127.0.0.1:{}", port), fmt::format("localhost:{}", port) };
	const std::string token = m_Token;

	server->set_pre_routing_handler([allowedHosts](const httplib::Request& req, httplib::Response& res)
		{
			// DNS rebinding: a page on evil.example that resolves to 127.0.0.1 still sends Host: evil.example
			const auto host = req.get_header_value("Host");
			if (host != allowedHosts[0] && host != allowedHosts[1])
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

	const auto Authorized = [token](const httplib::Request& req, httplib::Response& res)
		{
			if (ConstantTimeEquals(req.get_header_value("X-TF2PL-Token"), token))
				return true;
			res.status = 401;
			res.set_content(R"({"error":"unauthorized"})", "application/json");
			return false;
		};

	server->Get("/", [index](const httplib::Request&, httplib::Response& res)
		{
			res.set_header("Content-Security-Policy",
				"default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; "
				"connect-src 'self'; img-src 'self' data:; base-uri 'none'; form-action 'none'; frame-ancestors 'none'");
			res.set_content(index, "text/html; charset=utf-8");
		});

	server->Get("/api/state", [this, Authorized](const httplib::Request& req, httplib::Response& res)
		{
			if (!Authorized(req, res))
				return;
			std::lock_guard lock(m_Mutex);
			res.set_content(m_Snapshot, "application/json");
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

			std::lock_guard lock(m_Mutex);
			if (m_Actions.size() >= 64)
			{
				res.status = 429;
				res.set_content(R"({"error":"too many queued actions"})", "application/json");
				return;
			}
			m_Actions.push_back(req.body);
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
	}

	m_Port = port;
	m_Server = std::move(server);
	m_ThreadExited = std::make_shared<std::atomic_bool>(false);
	m_Thread = std::thread([server = m_Server.get(), exited = m_ThreadExited]()
		{
			server->listen_after_bind();
			*exited = true;
		});

	Log("Web UI listening on {}", GetURL());
	return true;
}

void WebUIController::Stop()
{
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
		if (Start(settings.m_WebUIPort) && settings.m_WebUIOpenBrowser && !m_BrowserOpened)
		{
			OpenInBrowser();
			m_BrowserOpened = true;
		}
	}

	if (!IsRunning())
		return;

	RunQueuedActions();

	if (now - m_LastSnapshot > 400ms)
	{
		m_LastSnapshot = now;
		std::string snapshot;
		try
		{
			snapshot = BuildSnapshot();
		}
		catch (const std::exception& e)
		{
			LogException(MH_SOURCE_LOCATION_CURRENT(), e, "Web UI: failed to build state snapshot");
			return;
		}

		std::lock_guard lock(m_Mutex);
		m_Snapshot = std::move(snapshot);
	}
}

std::string WebUIController::BuildSnapshot()
{
	auto& settings = m_Application.GetSettings();
	auto& world = m_Application.GetWorld();
	auto& modLogic = m_Application.GetModLogic();
	const SteamID localID = settings.GetLocalSteamID();
	const auto& colors = settings.m_Theme.m_Colors;

	nlohmann::json j;
	j["version"] = ++m_SnapshotVersion;
	j["appVersion"] = fmt::format("{}", VERSION);
	j["server"] = { { "name", world.GetServerHostName() }, { "map", world.GetMapName() } };
	j["local"] = std::to_string(localID.GetSteamID64());
	j["voteInProgress"] = world.IsVoteInProgress();

	auto& attributes = j["attributes"] = nlohmann::json::array();
	for (int i = 0; i < int(PlayerAttribute::COUNT); i++)
	{
		const auto attr = PlayerAttribute(i);
		const auto* color = AttributeColor(colors, attr);
		attributes.push_back({
			{ "id", nlohmann::json(attr) },
			{ "name", to_string(attr) },
			{ "color", color ? ColorHex(*color) : "#cccccc" },
		});
	}

	auto& customTags = j["customTags"] = nlohmann::json::array();
	if (auto* playerList = modLogic.GetPlayerList())
	{
		for (const auto& tag : playerList->GetCustomTags())
			customTags.push_back(tag);
	}

	auto& players = j["players"] = nlohmann::json::array();
	for (IPlayer& player : world.GetPlayers())
	{
		const SteamID id = player.GetSteamID();
		const auto marks = modLogic.GetPlayerAttributes(id);

		nlohmann::json p;
		p["steamid"] = std::to_string(id.GetSteamID64());
		p["steamid3"] = id.str();
		p["name"] = player.GetNameSafe();
		p["team"] = TeamName(player.GetTeam());
		p["state"] = StateName(player.GetConnectionState());
		p["ping"] = player.GetPing();
		p["kills"] = player.GetScores().m_Kills;
		p["deaths"] = player.GetScores().m_Deaths;
		p["connected"] = std::chrono::duration_cast<std::chrono::seconds>(player.GetConnectedTime()).count();
		p["friend"] = player.IsFriend();
		p["isLocal"] = (id == localID);
		if (auto userID = player.GetUserID())
			p["userid"] = *userID;

		auto& markList = p["marks"] = nlohmann::json::array();
		for (const auto& mark : marks)
			markList.push_back({ { "list", mark.m_FileName }, { "attrs", AttributesToJson(mark.m_Attributes) } });

		if (auto* playerList = modLogic.GetPlayerList())
			p["editable"] = AttributesToJson(playerList->GetEditablePlayerAttributes(id));

		p["canVotekick"] = id != localID && world.FindUserID(id).has_value() &&
			world.GetTeamShareResult(id, localID) == TeamShareResult::SameTeams;

		if (player.GetFriendsInfo().has_value())
		{
			const auto marked = modLogic.GetMarkedFriendsCount(player);
			p["markedFriends"] = { { "marked", marked.m_MarkedFriendsCountTotal }, { "total", marked.m_FriendsCountTotal } };
		}

		players.push_back(std::move(p));
	}

	auto& chat = j["chat"] = nlohmann::json::array();
	if (auto& mainState = m_Application.GetMainState())
	{
		size_t count = 0;
		for (const auto& line : mainState->m_PrintingLines) // newest first
		{
			if (!line || line->GetType() != ConsoleLineType::Chat)
				continue;

			const auto& chatLine = static_cast<const ChatConsoleLine&>(*line);
			chat.push_back({
				{ "name", chatLine.GetPlayerName() },
				{ "message", chatLine.GetMessage() },
				{ "steamid", std::to_string(chatLine.getSteamID().GetSteamID64()) },
				{ "team", chatLine.IsTeam() },
				{ "dead", chatLine.IsDead() },
				{ "self", chatLine.IsSelf() },
				{ "teamShare", TeamShareName(chatLine.GetTeamShareResult()) },
			});

			if (++count >= 150)
				break;
		}
	}

	// invalid UTF-8 in player names must not take the whole snapshot down
	return j.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
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
		m_LastSnapshot = {}; // send the result back on the next update
}

void WebUIController::RunAction(const nlohmann::json& action)
{
	auto& settings = m_Application.GetSettings();
	auto& world = m_Application.GetWorld();
	auto& modLogic = m_Application.GetModLogic();
	const std::string type = StringField(action, "type", 32);

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

	const auto id = SteamIDFromJson(action);
	if (!id)
		throw std::invalid_argument("missing or invalid steamid");
	if (*id == settings.GetLocalSteamID())
		throw std::invalid_argument("can't act on the local player");

	const IPlayer* player = world.FindPlayer(*id);
	const std::string name = player ? player->GetNameUnsafe() : std::string{};
	const std::string reason = StringField(action, "reason");
	const bool set = action.value("set", true);

	if (type == "mark")
	{
		const auto attr = action.at("attr").get<PlayerAttribute>(); // throws on unknown ids
		if (modLogic.SetPlayerAttribute(*id, name, attr, AttributePersistence::Saved, set, reason))
			Log("Web UI: {} {} {:v} | {}", name.empty() ? id->str() : name, set ? "marked" : "unmarked", mh::enum_fmt(attr), reason);
	}
	else if (type == "tag")
	{
		const std::string tag = StringField(action, "tag", 80);
		if (!PlayerAttributesList::IsValidCustomTag(tag))
			throw std::invalid_argument("invalid custom tag");
		modLogic.SetPlayerCustomTag(*id, name, tag, AttributePersistence::Saved, set, reason);
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
