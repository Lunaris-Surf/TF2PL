#include "WebUIData.h"
#include "Application.h"
#include "GenericErrors.h"
#include "Log.h"
#include "ModeratorLogic.h"
#include "PlayerListExport.h"
#include "PlayerStatus.h"
#include "PreviewMode.h"
#include "StreamerMode.h"
#include "UpdateManager.h"
#include "WorldState.h"
#include "Config/PlayerListJSON.h"
#include "Config/Settings.h"
#include "ConsoleLog/IConsoleLine.h"
#include "ConsoleLog/ConsoleLines/ChatConsoleLine.h"
#include "ConsoleLog/ConsoleLines/KillNotificationLine.h"
#include "ConsoleLog/ConsoleLines/SuicideNotificationLine.h"
#include "GameData/IPlayer.h"
#include "GameData/TFConstants.h"
#include "Networking/HTTPHelpers.h"
#include "Networking/LogsTFAPI.h"
#include "Networking/SteamAPI.h"
#include "Networking/SteamHistoryAPI.h"
#include "SetupFlow/AddonManagerPage.h"
#include "ReleaseChannel.h"
#include "Version.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <tf2pl/reflection/enum.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <deque>
#include <functional>
#include <map>
#include <set>

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace tf2_bot_detector;
using json = nlohmann::json;

namespace
{
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

	int ColorByte(float v) { return int(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); }

	std::string ColorHex(const std::array<float, 4>& c)
	{
		return fmt::format("#{:02x}{:02x}{:02x}", ColorByte(c[0]), ColorByte(c[1]), ColorByte(c[2]));
	}

	std::string ColorHex(float r, float g, float b)
	{
		return ColorHex(std::array<float, 4>{ r, g, b, 1 });
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

	json AttributesToJson(const PlayerAttributesList& attrs)
	{
		json out = json::array();
		for (size_t i = 0; i < PlayerAttributesList::size(); i++)
		{
			if (attrs.HasAttribute(PlayerAttribute(i)))
				out.push_back(json(PlayerAttribute(i)));
		}
		for (const auto& tag : attrs.GetCustomTags())
			out.push_back(tag);
		return out;
	}

	/// what the UI should say instead of a value we don't have
	json ErrorJson(const std::error_condition& err)
	{
		if (err == std::errc::operation_in_progress || err == tf2_bot_detector::ErrorCode::LazyValueUninitialized)
			return { { "status", "loading" } };
		if (err == SteamAPI::ErrorCode::EmptyAPIKey)
			return { { "status", "no_api_key" } };
		if (err == SteamAPI::ErrorCode::InfoPrivate || err == SteamAPI::ErrorCode::GameNotOwned ||
			err == HTTPResponseCode::Unauthorized || err == std::errc::permission_denied)
		{
			return { { "status", "private" } };
		}
		if (err == tf2_bot_detector::ErrorCode::InternetConnectivityDisabled || err == SteamAPI::ErrorCode::SteamAPIDisabled)
			return { { "status", "disabled" } };

		return { { "status", "error" }, { "message", err.message() } };
	}

	int64_t Days(duration_t d) { return std::chrono::duration_cast<std::chrono::hours>(d).count() / 24; }

	int64_t EpochSeconds(const time_point_t& t)
	{
		return std::chrono::duration_cast<std::chrono::seconds>(t.time_since_epoch()).count();
	}

	/// SteamID64 as the page knows it; your own is "self" in streamer mode so it never reaches the page
	std::string IdString(const Settings& settings, const SteamID& id)
	{
		if (!id.IsValid())
			return {};
		if (StreamerMode::IsMasked(settings, id))
			return "self";
		return std::to_string(id.GetSteamID64());
	}

	bool ContainsNoCase(std::string_view haystack, std::string_view needle)
	{
		if (needle.empty())
			return true;
		return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
			[](char a, char b) { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); }) != haystack.end();
	}

	std::string BestListName(const PlayerListJSON* playerList, const SteamID& id)
	{
		if (!playerList)
			return {};

		std::string best;
		std::optional<std::chrono::system_clock::time_point> bestTime;
		for (const auto& [fileName, data] : playerList->FindPlayerData(id))
		{
			if (data.m_LastSeen && !data.m_LastSeen->m_PlayerName.empty() &&
				(!bestTime || data.m_LastSeen->m_Time > *bestTime))
			{
				best = data.m_LastSeen->m_PlayerName;
				bestTime = data.m_LastSeen->m_Time;
			}
		}
		return best;
	}

	json ProofToJson(const std::vector<json>& proof)
	{
		json out = json::array();
		for (const auto& p : proof)
			out.push_back(p.is_string() ? p.get<std::string>() : p.dump());
		return out;
	}

	bool CanVotekick(TF2BDApplication& app, const SteamID& id)
	{
		if (app.IsPreviewMode())
			return false;
		const auto& world = app.GetWorld();
		const SteamID localID = app.GetSettings().GetLocalSteamID();
		return id != localID && world.FindUserID(id).has_value() &&
			world.GetTeamShareResult(id, localID) == TeamShareResult::SameTeams;
	}

	std::string EnumName(UpdateStatus status)
	{
		return fmt::format("{:v}", tf2pl::enum_fmt(status));
	}

	std::string FromPath(const std::filesystem::path& p)
	{
		const auto u8 = p.u8string();
		return std::string(u8.begin(), u8.end());
	}

	std::filesystem::path ToPath(const std::string& s)
	{
		return std::filesystem::path(std::u8string(s.begin(), s.end()));
	}

	json PathList(const std::vector<std::filesystem::path>& paths)
	{
		json out = json::array();
		for (const auto& p : paths)
			out.push_back(FromPath(p.filename()));
		return out;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// state

json WebUIData::BuildState(TF2BDApplication& app, const PlayerListExporter* exporter)
{
	auto& settings = app.GetSettings();
	auto& world = app.GetWorld();
	auto& modLogic = app.GetModLogic();
	auto* playerList = modLogic.GetPlayerList();
	const SteamID localID = settings.GetLocalSteamID();
	const auto& colors = settings.m_Theme.m_Colors;

	json j;
	j["appVersion"] = fmt::format("{}", VERSION);
	j["preview"] = { { "active", app.IsPreviewMode() }, { "requested", app.IsPreviewRequested() } };
	j["streamer"] = { { "enabled", settings.m_StreamerMode.m_Enabled }, { "name", StreamerMode::GetName(settings) } };
	j["paused"] = app.IsPaused();
	j["showCommands"] = settings.m_Unsaved.m_DebugShowCommands;
	j["killLogs"] = settings.m_KillLogsInChat;
	j["toggles"] =
	{
		{ "chatWarnings", settings.m_AutoChatWarnings },
		{ "partyWarnings", settings.m_AutoChatWarningsConnectingParty },
		{ "autoVotekick", settings.m_AutoVotekick },
		{ "autoMark", settings.m_AutoMark },
		{ "autoMarkBans", settings.m_AutoMarkBans },
	};
	{
		const auto& mw = settings.m_UIState.m_MainWindow;
		j["view"] =
		{
			{ "chat", mw.m_ChatEnabled }, { "appLog", mw.m_AppLogEnabled },
			{ "teamStats", mw.m_TeamStatsEnabled }, { "scoreboard", mw.m_ScoreboardEnabled },
		};
	}

	j["server"] = { { "name", world.GetServerHostName() }, { "map", world.GetMapName() } };
	j["voteInProgress"] = world.IsVoteInProgress();
	j["parseProgress"] = (app.GetMainState() && !app.IsPreviewMode()) ? app.GetMainState()->m_Parser.GetParseProgress() : 1.0f;
	j["rules"] = modLogic.GetRuleCount();
	j["blacklistedPlayers"] = modLogic.GetBlacklistedPlayerCount();
	j["webPort"] = settings.m_WebUIPort;

	// mark ids, names and the scoreboard colours the user picked
	auto& attributes = j["attributes"] = json::array();
	for (int i = 0; i < int(PlayerAttribute::COUNT); i++)
	{
		const auto attr = PlayerAttribute(i);
		const auto* color = AttributeColor(colors, attr);
		attributes.push_back({
			{ "id", json(attr) },
			{ "name", to_string(attr) },
			{ "color", color ? ColorHex(*color) : "#cccccc" },
		});
	}

	auto& customTags = j["customTags"] = json::array();
	auto& tagColors = j["tagColors"] = json::object();
	if (playerList)
	{
		for (const auto& tag : playerList->GetCustomTags())
		{
			customTags.push_back(tag);
			const auto found = settings.m_ScoreboardCustomTagBG.find(tag);
			tagColors[tag] = ColorHex(found == settings.m_ScoreboardCustomTagBG.end() ? colors.m_ScoreboardSuspiciousBG : found->second);
		}
	}

	j["colors"] =
	{
		{ "you", ColorHex(colors.m_ScoreboardYouFG) },
		{ "connecting", ColorHex(colors.m_ScoreboardConnectingFG) },
		{ "friendlyBG", ColorHex(colors.m_ScoreboardFriendlyTeamBG) },
		{ "friendlyBGAlpha", colors.m_ScoreboardFriendlyTeamBG[3] },
		{ "enemyBG", ColorHex(colors.m_ScoreboardEnemyTeamBG) },
		{ "enemyBGAlpha", colors.m_ScoreboardEnemyTeamBG[3] },
		{ "chatYou", ColorHex(colors.m_ChatLogYouFG) },
		{ "chatFriendly", ColorHex(colors.m_ChatLogFriendlyTeamFG) },
		{ "chatEnemy", ColorHex(colors.m_ChatLogEnemyTeamFG) },
	};

	auto& sites = j["profileSites"] = json::array();
	for (const auto& site : settings.m_GotoProfileSites)
		sites.push_back({ { "name", site.m_Name }, { "url", site.m_ProfileURL } });

	// the same players the native scoreboard shows
	auto& players = j["players"] = json::array();
	size_t markedCount = 0;
	if (auto& mainState = app.GetMainState())
	{
		for (IPlayer& player : mainState->GeneratePlayerPrintData())
		{
			const SteamID id = player.GetSteamID();
			const bool masked = StreamerMode::IsMasked(settings, id);
			const auto marks = modLogic.GetPlayerAttributes(id);
			if (!marks.empty())
				markedCount++;

			json p;
			p["id"] = IdString(settings, id);
			p["steamid3"] = StreamerMode::DisplaySteamID(settings, id);
			p["name"] = masked ? StreamerMode::GetName(settings) : player.GetNameSafe();
			p["team"] = TeamName(player.GetTeam());
			p["teamShare"] = TeamShareName(modLogic.GetTeamShareResult(id));
			p["state"] = StateName(player.GetConnectionState());
			p["ping"] = player.GetPing();
			p["kills"] = player.GetScores().m_Kills;
			p["deaths"] = player.GetScores().m_Deaths;
			p["connected"] = std::chrono::duration_cast<std::chrono::seconds>(player.GetConnectedTime()).count();
			p["friend"] = player.IsFriend();
			p["isLocal"] = (id == localID);
			p["masked"] = masked;
			if (auto userID = player.GetUserID())
				p["userid"] = *userID;

			auto& markList = p["marks"] = json::array();
			for (const auto& mark : marks)
				markList.push_back({ { "list", mark.m_FileName }, { "attrs", AttributesToJson(mark.m_Attributes) } });

			p["editable"] = AttributesToJson(app.GetEditablePlayerAttributes(id));
			p["canVotekick"] = CanVotekick(app, id);

			// the scoreboard's icons
			if (const auto& bans = player.GetPlayerBans(); bans.has_value())
			{
				p["vac"] = bans->m_VACBanCount;
				p["gameBans"] = bans->m_GameBanCount;
			}
			if (const auto& sourceBans = player.GetPlayerSourceBanState(); sourceBans.has_value())
				p["sourceBans"] = sourceBans->size();

			if (player.GetFriendsInfo().has_value())
			{
				const auto marked = modLogic.GetMarkedFriendsCount(player);
				p["markedFriends"] = { { "marked", marked.m_MarkedFriendsCountTotal }, { "total", marked.m_FriendsCountTotal } };
			}

			players.push_back(std::move(p));
		}
	}
	j["markedCount"] = markedCount;

	// Player Lists summary
	if (playerList)
	{
		j["lists"] =
		{
			{ "entries", playerList->GetPlayerCount() },
			{ "loading", playerList->ArePlayerListsLoading() },
			{ "writable", playerList->GetWritableListName() },
		};
	}

	// update check
	if (auto* updates = app.GetUpdateManager())
	{
		const auto status = updates->GetUpdateStatus().get();
		json u = { { "status", EnumName(status.m_Status) }, { "message", status.m_Message } };
		if (const auto* available = updates->GetAvailableUpdate())
		{
			u["available"] =
			{
				{ "version", fmt::format("{}", available->m_BuildInfo.m_Version) },
				{ "url", available->m_BuildInfo.m_GitHubURL },
			};
		}
		j["update"] = std::move(u);
	}

	// untrusted / blacklisted list warning (shared with the native popup)
	if (const auto& report = app.GetUntrustedListReport(); !report.empty())
	{
		j["untrusted"] =
		{
			{ "blacklistedDeleted", PathList(report.m_BlacklistedDeleted) },
			{ "blacklistedDisabled", PathList(report.m_BlacklistedDisabled) },
			{ "blacklistedKept", PathList(report.m_BlacklistedKept) },
			{ "deleted", PathList(report.m_Deleted) },
			{ "contaminated", PathList(report.m_Contaminated) },
			{ "exports", PathList(report.m_PossiblyContaminatedExports) },
		};
	}

	// Total Export
	if (exporter)
	{
		const auto progress = exporter->GetProgress();
		const char* stage = "idle";
		switch (progress.m_Stage)
		{
		case PlayerListExporter::Progress::Stage::Idle:       stage = "idle"; break;
		case PlayerListExporter::Progress::Stage::Gathering:  stage = "gathering"; break;
		case PlayerListExporter::Progress::Stage::Summaries:  stage = "summaries"; break;
		case PlayerListExporter::Progress::Stage::Bans:       stage = "bans"; break;
		case PlayerListExporter::Progress::Stage::SourceBans: stage = "sourcebans"; break;
		case PlayerListExporter::Progress::Stage::Writing:    stage = "writing"; break;
		case PlayerListExporter::Progress::Stage::Finished:   stage = "finished"; break;
		case PlayerListExporter::Progress::Stage::Failed:     stage = "failed"; break;
		}
		j["export"] =
		{
			{ "running", exporter->IsRunning() },
			{ "stage", stage },
			{ "completed", progress.m_Completed },
			{ "total", progress.m_Total },
			{ "message", progress.m_Message },
			{ "removedNonexistent", progress.m_RemovedNonexistent },
			{ "filledNames", progress.m_FilledNames },
			{ "vacBanned", progress.m_VACBanned },
			{ "gameBanned", progress.m_GameBanned },
			{ "sourceBanned", progress.m_SourceBanned },
		};
	}

	return j;
}

// ---------------------------------------------------------------------------------------------------------------------
// chat / log

json WebUIData::BuildChat(TF2BDApplication& app)
{
	const auto& settings = app.GetSettings();
	const bool killLogs = settings.m_KillLogsInChat;

	std::vector<json> lines;
	for (const auto& line : app.GetDisplayedConsoleLines()) // newest first
	{
		if (!line)
			continue;

		json l;
		switch (line->GetType())
		{
		case ConsoleLineType::Chat:
		{
			const auto& chat = static_cast<const ChatConsoleLine&>(*line);
			const bool maskSelf = chat.IsSelf() && StreamerMode::IsEnabled(settings);
			l =
			{
				{ "kind", "chat" },
				{ "name", maskSelf ? StreamerMode::GetName(settings) : StreamerMode::DisplayName(settings, chat.getSteamID(), chat.GetPlayerName()) },
				{ "message", chat.GetMessage() },
				{ "id", maskSelf ? "self"s : IdString(settings, chat.getSteamID()) },
				{ "team", chat.IsTeam() },
				{ "dead", chat.IsDead() },
				{ "self", chat.IsSelf() },
				{ "teamShare", TeamShareName(chat.GetTeamShareResult()) },
			};
			break;
		}
		case ConsoleLineType::KillNotification:
		{
			if (!killLogs)
				continue;
			const auto& kill = static_cast<const KillNotificationLine&>(*line);
			l =
			{
				{ "kind", "kill" },
				{ "attacker", StreamerMode::DisplayName(settings, kill.GetAttacker(), kill.GetAttackerName()) },
				{ "attackerId", IdString(settings, kill.GetAttacker()) },
				{ "victim", StreamerMode::DisplayName(settings, kill.GetVictim(), kill.GetVictimName()) },
				{ "victimId", IdString(settings, kill.GetVictim()) },
				{ "weapon", kill.GetWeaponName() },
				{ "crit", kill.WasCrit() },
				{ "self", kill.GetAttacker() == settings.GetLocalSteamID() },
			};
			break;
		}
		case ConsoleLineType::SuicideNotification:
		{
			if (!killLogs)
				continue;
			const auto& suicide = static_cast<const SuicideNotificationLine&>(*line);
			l =
			{
				{ "kind", "suicide" },
				{ "name", StreamerMode::DisplayName(settings, suicide.GetId(), suicide.GetName()) },
				{ "id", IdString(settings, suicide.GetId()) },
			};
			break;
		}
		default:
			continue;
		}

		l["t"] = EpochSeconds(line->GetTimestamp());
		lines.push_back(std::move(l));
		if (lines.size() >= 200)
			break;
	}

	json out = json::array();
	for (auto it = lines.rbegin(); it != lines.rend(); ++it)
		out.push_back(std::move(*it));
	return out;
}

json WebUIData::BuildLog()
{
	std::deque<json> recent;
	for (const LogMessage& msg : ILogManager::GetInstance().GetVisibleMsgs())
	{
		recent.push_back({
			{ "t", std::chrono::duration_cast<std::chrono::milliseconds>(msg.m_Timestamp.time_since_epoch()).count() },
			{ "text", msg.m_Text },
			{ "color", ColorHex(msg.m_Color.r, msg.m_Color.g, msg.m_Color.b) },
			{ "alpha", msg.m_Color.a },
		});
		if (recent.size() > 300)
			recent.pop_front();
	}

	json out = json::array();
	for (auto& line : recent)
		out.push_back(std::move(line));
	return out;
}

// ---------------------------------------------------------------------------------------------------------------------
// dossier

json WebUIData::BuildDossier(TF2BDApplication& app, const SteamID& id)
{
	auto& settings = app.GetSettings();
	auto& world = app.GetWorld();
	auto& modLogic = app.GetModLogic();
	auto* playerList = modLogic.GetPlayerList();
	auto* preview = app.GetPreview();
	const bool samplePlayer = preview && preview->OwnsPlayer(id);

	json d;
	d["id"] = IdString(settings, id);

	if (StreamerMode::IsMasked(settings, id))
	{
		// your own dossier in streamer mode: just the custom profile
		d["masked"] = true;
		d["name"] = StreamerMode::GetName(settings);
		d["steamid3"] = StreamerMode::GetSteamIDText(settings);
		d["isLocal"] = true;
		d["inGame"] = world.FindPlayer(id) != nullptr;
		d["hasCustomAvatar"] = !settings.m_StreamerMode.m_AvatarPath.empty();
		return d;
	}

	const IPlayer* player = world.FindPlayer(id);
	d["masked"] = false;
	d["inGame"] = player != nullptr;
	d["isLocal"] = id == settings.GetLocalSteamID();
	d["sample"] = samplePlayer;
	d["steamid3"] = id.str();
	d["steamid64"] = std::to_string(id.GetSteamID64());
	d["name"] = player ? player->GetNameSafe() : BestListName(playerList, id);
	d["canVotekick"] = CanVotekick(app, id);

	if (player)
	{
		if (auto userID = player->GetUserID())
			d["userid"] = *userID;
		d["team"] = TeamName(player->GetTeam());
		d["state"] = StateName(player->GetConnectionState());

		// Steam profile
		if (const auto& summary = player->GetPlayerSummary(); summary.has_value())
		{
			json s = { { "status", "ok" }, { "nickname", summary->m_Nickname }, { "realName", summary->m_RealName },
				{ "vanity", std::string(summary->GetVanityURL()) }, { "configured", summary->m_ProfileConfigured } };
			switch (summary->m_Visibility)
			{
			case SteamAPI::CommunityVisibilityState::Public:      s["visibility"] = "public"; break;
			case SteamAPI::CommunityVisibilityState::FriendsOnly: s["visibility"] = "friends"; break;
			case SteamAPI::CommunityVisibilityState::Private:     s["visibility"] = "private"; break;
			default:                                              s["visibility"] = "unknown"; break;
			}
			if (auto age = summary->GetAccountAge())
				s["accountAgeDays"] = Days(*age);
			if (!samplePlayer && !summary->m_AvatarHash.empty())
				d["avatar"] = summary->GetAvatarURL(SteamAPI::AvatarQuality::Large);
			d["summary"] = std::move(s);
		}
		else
		{
			d["summary"] = ErrorJson(summary.error());
		}
		if (auto estimated = player->GetEstimatedAccountAge())
			d["estimatedAgeDays"] = Days(*estimated);

		if (const auto& bans = player->GetPlayerBans(); bans.has_value())
		{
			const char* economy = "none";
			switch (bans->m_EconomyBan)
			{
			case SteamAPI::PlayerEconomyBan::None:      economy = "none"; break;
			case SteamAPI::PlayerEconomyBan::Probation: economy = "probation"; break;
			case SteamAPI::PlayerEconomyBan::Banned:    economy = "banned"; break;
			case SteamAPI::PlayerEconomyBan::Unknown:   economy = "unknown"; break;
			}
			d["bans"] =
			{
				{ "status", "ok" }, { "vac", bans->m_VACBanCount }, { "game", bans->m_GameBanCount },
				{ "community", bans->m_CommunityBanned }, { "economy", economy },
				{ "daysSinceLast", Days(bans->m_TimeSinceLastBan) },
			};
		}
		else
		{
			d["bans"] = ErrorJson(bans.error());
		}

		if (auto playtime = player->GetTF2Playtime(); playtime.has_value())
			d["playtime"] = { { "status", "ok" }, { "hours", std::chrono::duration_cast<std::chrono::hours>(playtime.value()).count() } };
		else
			d["playtime"] = ErrorJson(playtime.error());

		if (const auto& logs = player->GetLogsInfo(); logs.has_value())
			d["logs"] = { { "status", "ok" }, { "count", logs->m_LogsCount } };
		else
			d["logs"] = ErrorJson(logs.error());

		if (const auto& inventory = player->GetInventoryInfo(); inventory.has_value())
			d["inventory"] = { { "status", "ok" }, { "items", inventory->m_Items }, { "slots", inventory->m_Slots } };
		else
			d["inventory"] = ErrorJson(inventory.error());

		if (const auto& sourceBans = player->GetPlayerSourceBanState(); sourceBans.has_value())
		{
			json list = json::array();
			for (const auto& [server, ban] : sourceBans.value())
			{
				const char* state = "expired";
				switch (ban.m_BanState)
				{
				case SteamHistoryAPI::Expired:   state = "expired"; break;
				case SteamHistoryAPI::Unbanned:  state = "unbanned"; break;
				case SteamHistoryAPI::Current:   state = "current"; break;
				case SteamHistoryAPI::Permanent: state = "permanent"; break;
				}
				list.push_back({
					{ "server", server }, { "name", ban.m_UserName }, { "state", state },
					{ "reason", ban.m_BanReason }, { "daysAgo", Days(tfbd_clock_t::now() - ban.m_BanTimestamp) },
				});
			}
			d["sourceBans"] = { { "status", "ok" }, { "list", std::move(list) } };
		}
		else
		{
			d["sourceBans"] = ErrorJson(sourceBans.error());
		}

		// friends: counts by mark and the marked people themselves (native: tooltip + Marked Friends window)
		if (const auto& friends = player->GetFriendsInfo(); friends.has_value())
		{
			const auto counts = modLogic.GetMarkedFriendsCount(const_cast<IPlayer&>(*player));
			json byMark = json::object();
			for (const auto& [attr, count] : counts.m_MarkedFriendsCount)
			{
				if (count > 0)
					byMark[to_string(attr)] = count;
			}

			json list = json::array();
			for (const SteamID& friendID : friends->m_Friends)
			{
				const auto marks = modLogic.GetPlayerAttributes(friendID);
				if (marks.empty())
					continue;

				PlayerAttributesList combined;
				json lists = json::array();
				for (const auto& mark : marks)
				{
					combined |= mark.m_Attributes;
					lists.push_back(mark.m_FileName);
				}

				const IPlayer* inGame = world.FindPlayer(friendID);
				list.push_back({
					{ "id", IdString(settings, friendID) },
					{ "steamid3", StreamerMode::DisplaySteamID(settings, friendID) },
					{ "name", StreamerMode::IsMasked(settings, friendID) ? StreamerMode::GetName(settings) :
						(inGame ? inGame->GetNameSafe() : BestListName(playerList, friendID)) },
					{ "inGame", inGame != nullptr },
					{ "attrs", AttributesToJson(combined) },
					{ "lists", std::move(lists) },
				});
			}

			d["friends"] =
			{
				{ "status", "ok" },
				{ "total", friends->m_Friends.size() },
				{ "marked", list.size() },
				{ "byMark", std::move(byMark) },
				{ "list", std::move(list) },
			};
		}
		else
		{
			d["friends"] = ErrorJson(friends.error());
		}

		if (modLogic.GetTeamShareResult(id) != TeamShareResult::SameTeams)
			d["thirst"] = { { "kills", player->GetScores().m_LocalKills }, { "deaths", player->GetScores().m_LocalDeaths } };
	}

	// marks by list
	json byList = json::array();
	const std::string writable = playerList ? playerList->GetWritableListName() : std::string{};
	if (samplePlayer)
	{
		for (const auto& mark : modLogic.GetPlayerAttributes(id))
			byList.push_back({ { "list", mark.m_FileName }, { "writable", false }, { "attrs", AttributesToJson(mark.m_Attributes) }, { "proof", json::array() } });
	}
	else if (playerList)
	{
		for (const auto& [fileName, data] : playerList->FindPlayerData(id))
		{
			const auto attrs = data.GetAttributes();
			if (attrs.empty())
				continue;

			json entry =
			{
				{ "list", fileName },
				{ "writable", fileName == writable },
				{ "attrs", AttributesToJson(attrs) },
				{ "proof", ProofToJson(data.m_Proof) },
			};
			if (data.m_LastSeen)
			{
				entry["lastSeen"] =
				{
					{ "t", std::chrono::duration_cast<std::chrono::seconds>(data.m_LastSeen->m_Time.time_since_epoch()).count() },
					{ "name", data.m_LastSeen->m_PlayerName },
				};
			}
			byList.push_back(std::move(entry));
		}
	}
	d["lists"] = std::move(byList);
	d["editable"] = AttributesToJson(app.GetEditablePlayerAttributes(id));

	return d;
}

// ---------------------------------------------------------------------------------------------------------------------
// player lists browser

WebUIData::ListQuery WebUIData::ParseListQuery(const json& action)
{
	ListQuery q;
	const auto Str = [&](const char* name, size_t max)
	{
		auto found = action.find(name);
		if (found == action.end() || !found->is_string())
			return std::string{};
		auto value = found->get<std::string>();
		if (value.size() > max)
			value.resize(max);
		return value;
	};
	q.m_Search = Str("search", 128);
	q.m_List = Str("list", 260);
	q.m_Mark = Str("mark", 80);
	q.m_OnlyWritable = action.value("onlyWritable", false);
	q.m_Offset = std::min<size_t>(action.value("offset", size_t(0)), 10'000'000);
	q.m_Limit = std::clamp<size_t>(action.value("limit", size_t(200)), 1, 500);
	return q;
}

json WebUIData::BuildLists(TF2BDApplication& app, const ListQuery& query)
{
	auto& settings = app.GetSettings();
	auto* playerList = app.GetModLogic().GetPlayerList();

	json j;
	j["query"] =
	{
		{ "search", query.m_Search }, { "list", query.m_List }, { "mark", query.m_Mark },
		{ "onlyWritable", query.m_OnlyWritable }, { "offset", query.m_Offset }, { "limit", query.m_Limit },
	};
	j["rows"] = json::array();
	j["lists"] = json::array();
	j["matched"] = 0;
	j["total"] = 0;
	if (!playerList)
		return j;

	const std::string writable = playerList->GetWritableListName();
	j["writable"] = writable;
	j["loading"] = playerList->ArePlayerListsLoading();

	std::optional<PlayerAttribute> markFilter;
	std::string tagFilter;
	if (!query.m_Mark.empty())
	{
		if (query.m_Mark.starts_with("custom:"))
			tagFilter = query.m_Mark;
		else
		{
			try { markFilter = json(query.m_Mark).get<PlayerAttribute>(); }
			catch (...) {}
		}
	}

	std::set<std::string> lists;
	size_t total = 0;
	size_t matched = 0;
	auto& rows = j["rows"];
	for (const auto& [fileName, data] : playerList->GetAllPlayerData())
	{
		const auto attrs = data.GetAttributes();
		if (attrs.empty())
			continue; // placeholder entries are not saved and not worth listing

		total++;
		lists.insert(fileName);

		if (query.m_OnlyWritable && fileName != writable)
			continue;
		if (!query.m_List.empty() && fileName != query.m_List)
			continue;
		if (markFilter && !attrs.HasAttribute(*markFilter))
			continue;
		if (!tagFilter.empty() && !attrs.GetCustomTags().contains(tagFilter))
			continue;

		const SteamID id = data.GetSteamID();
		const bool masked = StreamerMode::IsMasked(settings, id);
		const std::string lastName = data.m_LastSeen ? data.m_LastSeen->m_PlayerName : std::string{};
		if (!query.m_Search.empty())
		{
			if (masked ||
				(!ContainsNoCase(lastName, query.m_Search) &&
				!ContainsNoCase(id.str(), query.m_Search) &&
				!ContainsNoCase(std::to_string(id.GetSteamID64()), query.m_Search)))
			{
				continue;
			}
		}

		if (matched >= query.m_Offset && rows.size() < query.m_Limit)
		{
			json row =
			{
				{ "id", IdString(settings, id) },
				{ "steamid3", StreamerMode::DisplaySteamID(settings, id) },
				{ "name", masked ? StreamerMode::GetName(settings) : lastName },
				{ "list", fileName },
				{ "writable", fileName == writable },
				{ "attrs", AttributesToJson(attrs) },
				{ "proofCount", data.m_Proof.size() },
			};
			if (data.m_LastSeen)
				row["lastSeen"] = std::chrono::duration_cast<std::chrono::seconds>(data.m_LastSeen->m_Time.time_since_epoch()).count();
			rows.push_back(std::move(row));
		}
		matched++;
	}

	for (const auto& list : lists)
		j["lists"].push_back(list);
	j["matched"] = matched;
	j["total"] = total;
	return j;
}

std::optional<SteamID> WebUIData::ParseSteamID(std::string text)
{
	std::erase_if(text, [](char c) { return std::isspace((unsigned char)c); });
	if (text.empty())
		return std::nullopt;

	// profile links: .../profiles/7656xxxxxxxxxxxxx[/]
	if (auto pos = text.find("/profiles/"); pos != text.npos)
	{
		text = text.substr(pos + 10);
		if (auto slash = text.find('/'); slash != text.npos)
			text.resize(slash);
	}

	try
	{
		SteamID id(text);
		if (id.IsValid())
			return id;
	}
	catch (...)
	{
	}
	return std::nullopt;
}

// ---------------------------------------------------------------------------------------------------------------------
// settings registry

namespace
{
	enum class SettingType
	{
		Bool,
		Int,
		Float,
		String,
		Secret,    // write-only (API keys): the page only learns whether one is set
		Enum,
		Color,
		Path,      // string with a "Browse..." action on the native side
	};

	const char* SettingTypeName(SettingType type)
	{
		switch (type)
		{
		case SettingType::Bool:   return "bool";
		case SettingType::Int:    return "int";
		case SettingType::Float:  return "float";
		case SettingType::String: return "string";
		case SettingType::Secret: return "secret";
		case SettingType::Enum:   return "enum";
		case SettingType::Color:  return "color";
		case SettingType::Path:   return "path";
		}
		return "string";
	}

	struct SettingDesc
	{
		std::string m_ID;
		std::string m_Section;
		std::string m_Label;
		std::string m_Help;
		SettingType m_Type = SettingType::Bool;
		double m_Min = 0;
		double m_Max = 0;
		double m_Step = 1;
		std::vector<std::pair<std::string, std::string>> m_Options; // value, label
		std::function<json(const Settings&)> m_Get;
		std::function<void(Settings&, const json&)> m_Set;
		std::function<bool(const Settings&)> m_Visible;
		std::function<std::string(const Settings&)> m_Placeholder;
	};

	bool ToBool(const json& v)
	{
		if (!v.is_boolean())
			throw std::invalid_argument("expected true/false");
		return v.get<bool>();
	}

	double ToNumber(const json& v, double min, double max)
	{
		if (!v.is_number())
			throw std::invalid_argument("expected a number");
		const double d = v.get<double>();
		if (d < min || d > max)
			throw std::invalid_argument(fmt::format("must be between {} and {}", min, max));
		return d;
	}

	std::string ToString(const json& v, size_t maxLength = 1024)
	{
		if (!v.is_string())
			throw std::invalid_argument("expected text");
		std::string s = v.get<std::string>();
		if (s.size() > maxLength)
			throw std::invalid_argument("text is too long");
		return s;
	}

	using ColorMember = std::array<float, 4> Settings::Theme::Colors::*;
	struct ColorDesc
	{
		const char* m_Key;
		const char* m_Label;
		ColorMember m_Member;
	};
	constexpr ColorDesc COLOR_SETTINGS[] =
	{
		{ "cheater", "Cheater", &Settings::Theme::Colors::m_ScoreboardCheaterBG },
		{ "suspicious", "Suspicious", &Settings::Theme::Colors::m_ScoreboardSuspiciousBG },
		{ "exploiter", "Exploiter", &Settings::Theme::Colors::m_ScoreboardExploiterBG },
		{ "racist", "Racist", &Settings::Theme::Colors::m_ScoreboardRacistBG },
		{ "hostile", "Hostile", &Settings::Theme::Colors::m_ScoreboardHostileBG },
		{ "suspected_cheater", "Suspected cheater", &Settings::Theme::Colors::m_ScoreboardSuspectedCheaterBG },
		{ "blacklisted", "Blacklisted", &Settings::Theme::Colors::m_ScoreboardBlacklistedBG },
		{ "vac_banned", "VAC banned", &Settings::Theme::Colors::m_ScoreboardVACBannedBG },
		{ "game_banned", "Game banned", &Settings::Theme::Colors::m_ScoreboardGameBannedBG },
		{ "source_banned", "Source banned", &Settings::Theme::Colors::m_ScoreboardSourceBannedBG },
		{ "pedophilia", "Pedophilia", &Settings::Theme::Colors::m_ScoreboardPedophiliaBG },
		{ "untrusted_list_user", "Untrusted list user", &Settings::Theme::Colors::m_ScoreboardUntrustedListUserBG },
		{ "zoophilia", "Zoophile/Zoo jokes", &Settings::Theme::Colors::m_ScoreboardZoophiliaBG },
		{ "you", "Scoreboard: you", &Settings::Theme::Colors::m_ScoreboardYouFG },
		{ "connecting", "Scoreboard: connecting", &Settings::Theme::Colors::m_ScoreboardConnectingFG },
		{ "friendly_team", "Scoreboard: friendly team", &Settings::Theme::Colors::m_ScoreboardFriendlyTeamBG },
		{ "enemy_team", "Scoreboard: enemy team", &Settings::Theme::Colors::m_ScoreboardEnemyTeamBG },
		{ "chat_you", "Chat: you", &Settings::Theme::Colors::m_ChatLogYouFG },
		{ "chat_friendly", "Chat: friendly team", &Settings::Theme::Colors::m_ChatLogFriendlyTeamFG },
		{ "chat_enemy", "Chat: enemy team", &Settings::Theme::Colors::m_ChatLogEnemyTeamFG },
	};

	json ColorToJson(const std::array<float, 4>& c)
	{
		return { { "hex", ColorHex(c) }, { "alpha", c[3] } };
	}

	std::array<float, 4> ColorFromJson(const json& v)
	{
		if (!v.is_object())
			throw std::invalid_argument("expected a colour");
		const std::string hex = v.value("hex", "");
		if (hex.size() != 7 || hex[0] != '#' || !std::all_of(hex.begin() + 1, hex.end(), [](char c) { return std::isxdigit((unsigned char)c); }))
			throw std::invalid_argument("expected #rrggbb");

		const auto Channel = [&](size_t offset) { return float(std::stoi(hex.substr(offset, 2), nullptr, 16)) / 255.0f; };
		const float alpha = float(std::clamp(v.value("alpha", 1.0), 0.0, 1.0));
		return { Channel(1), Channel(3), Channel(5), alpha };
	}

	// helpers to keep the table below readable
	SettingDesc Bool(std::string id, std::string section, std::string label, std::string help, bool GeneralSettings::* member)
	{
		SettingDesc d{ std::move(id), std::move(section), std::move(label), std::move(help), SettingType::Bool };
		d.m_Get = [member](const Settings& s) { return json(s.*member); };
		d.m_Set = [member](Settings& s, const json& v) { s.*member = ToBool(v); };
		return d;
	}

	SettingDesc Int(std::string id, std::string section, std::string label, std::string help, int GeneralSettings::* member, int min, int max)
	{
		SettingDesc d{ std::move(id), std::move(section), std::move(label), std::move(help), SettingType::Int, double(min), double(max) };
		d.m_Get = [member](const Settings& s) { return json(s.*member); };
		d.m_Set = [member, min, max](Settings& s, const json& v) { s.*member = int(ToNumber(v, min, max)); };
		return d;
	}

	SettingDesc Text(std::string id, std::string section, std::string label, std::string help, std::string GeneralSettings::* member)
	{
		SettingDesc d{ std::move(id), std::move(section), std::move(label), std::move(help), SettingType::String };
		d.m_Get = [member](const Settings& s) { return json(s.*member); };
		d.m_Set = [member](Settings& s, const json& v) { s.*member = ToString(v); };
		return d;
	}

	const std::vector<SettingDesc>& GetSettingDescs()
	{
		static const std::vector<SettingDesc> s_Descs = []
		{
			std::vector<SettingDesc> v;
			const auto Add = [&](SettingDesc d) -> SettingDesc& { v.push_back(std::move(d)); return v.back(); };

			// --- Streamer mode -------------------------------------------------------------------------------------
			{
				const std::string section = "Streamer mode";
				{
					auto& d = Add({ "streamer.enabled", section, "Enable streamer mode",
						"Shows this profile instead of your own name, avatar and SteamID everywhere TF2PL draws you (scoreboard, dossier, chat, kill feed, overlay, this page) and hides your profile links.",
						SettingType::Bool });
					d.m_Get = [](const Settings& s) { return json(s.m_StreamerMode.m_Enabled); };
					d.m_Set = [](Settings& s, const json& j) { s.m_StreamerMode.m_Enabled = ToBool(j); };
				}
				{
					auto& d = Add({ "streamer.name", section, "Shown name", "Leave empty for \"Anonymous\".", SettingType::String });
					d.m_Get = [](const Settings& s) { return json(s.m_StreamerMode.m_Name); };
					d.m_Set = [](Settings& s, const json& j) { s.m_StreamerMode.m_Name = ToString(j, 128); };
					d.m_Placeholder = [](const Settings&) { return std::string(StreamerMode::DEFAULT_NAME); };
				}
				{
					auto& d = Add({ "streamer.steamid", section, "Shown SteamID",
						"Any text, e.g. a fake SteamID. Leave empty to show \"Hidden\". Copying your SteamID and profile links stay off for you.",
						SettingType::String });
					d.m_Get = [](const Settings& s) { return json(s.m_StreamerMode.m_SteamID); };
					d.m_Set = [](Settings& s, const json& j) { s.m_StreamerMode.m_SteamID = ToString(j, 128); };
					d.m_Placeholder = [](const Settings&) { return std::string(StreamerMode::HIDDEN_STEAMID); };
				}
				{
					auto& d = Add({ "streamer.avatar", section, "Avatar image",
						"A png/jpg/bmp/tga/gif on this PC shown as your avatar. Leave empty for a blank avatar. Square images look best.",
						SettingType::Path });
					d.m_Get = [](const Settings& s) { return json(s.m_StreamerMode.m_AvatarPath); };
					d.m_Set = [](Settings& s, const json& j) { s.m_StreamerMode.m_AvatarPath = ToString(j); };
					d.m_Placeholder = [](const Settings&) { return std::string("blank avatar"); };
				}
			}

			// --- Moderation -----------------------------------------------------------------------------------------
			{
				const std::string section = "Moderation";
				Add(Bool("auto_chat_warnings", section, "Chat warnings", "Sends chat message warnings about cheaters.", &GeneralSettings::m_AutoChatWarnings));
				Add(Bool("auto_votekick", section, "Auto votekick", "Automatically votekicks cheaters on your team.", &GeneralSettings::m_AutoVotekick));
				Add(Bool("auto_mark", section, "Auto-mark", "Automatically marks players matching the detection rules.", &GeneralSettings::m_AutoMark));
				Add(Bool("auto_mark_bans", section, "Mark ban records",
					"Requires Auto-mark. Marks VAC/game ban history and SourceBans; scans SourceBan reasons with rules whose sources include sourcebans.",
					&GeneralSettings::m_AutoMarkBans));
				Add(Bool("auto_temp_mute", section, "Auto temp mute",
					"Temporarily mutes in-game chat warnings if we think someone else in the server is running the tool.", &GeneralSettings::m_AutoTempMute));
				{
					auto& d = Add({ "auto_votekick_delay", section, "Auto votekick delay (seconds)",
						"Delay between a player fully connecting and us expecting them to be able to vote. Players can't vote until they've picked a team and class.",
						SettingType::Float, 0, 30, 0.5 });
					d.m_Get = [](const Settings& s) { return json(s.m_AutoVotekickDelay); };
					d.m_Set = [](Settings& s, const json& j) { s.m_AutoVotekickDelay = float(ToNumber(j, 0, 30)); };
				}
				Add(Int("min_vote_kick_interval", section, "Minimum votekick interval (seconds)", "Waits this long between auto votekick attempts.",
					&GeneralSettings::m_MinVoteKickInterval, 5, 120));
				Add(Bool("vote_kick_ignore_team_state", section, "[Experimental] Votekick ignores team state on vsh_/ze_ maps", "",
					&GeneralSettings::m_VoteKickIgnoreTeamStateOnCertainMaps));
				Add(Bool("auto_chat_warnings_connecting", section, "Chat warnings for connecting cheaters",
					"Sends a chat message when a known cheater joins the lobby but isn't in the game yet. Needs Chat warnings.",
					&GeneralSettings::m_AutoChatWarningsConnecting));
				Add(Bool("chat_warning_warn_once", section, "Send chat warnings once per player", "", &GeneralSettings::m_ChatWarningSendOnce));
				{
					auto& d = v.emplace_back(Int("chat_warning_interval", section, "Chat warning frequency (seconds)",
						"Delay between chat warnings. Under 10 seconds is considered chat spam by most players - expect to be kicked yourself.",
						&GeneralSettings::m_ChatWarningInterval, 2, 60));
					d.m_Visible = [](const Settings& s) { return !s.m_ChatWarningSendOnce; };
				}
				Add(Bool("use_custom_chat_warnings", section, "Custom chat message warnings",
					"Other TF2PL users won't recognise custom messages, which can cause duplicate warnings. Use {} for the name / count.",
					&GeneralSettings::m_UseCustomChatWarnings));
				const auto CustomOnly = [](const Settings& s) { return s.m_UseCustomChatWarnings; };
				v.push_back(Text("one_cheater_connecting", section, "Message: cheater joining", "", &GeneralSettings::m_OneCheaterConnectingMessage)); v.back().m_Visible = CustomOnly;
				v.push_back(Text("muti_cheater_connecting", section, "Message: multiple cheaters joining", "{} = count", &GeneralSettings::m_MultipleCheaterConnectingMessage)); v.back().m_Visible = CustomOnly;
				v.push_back(Text("one_cheater_warning", section, "Message: cheater warning", "{} = name", &GeneralSettings::m_OneCheaterWarningMessage)); v.back().m_Visible = CustomOnly;
				v.push_back(Text("muti_cheater_warning", section, "Message: multiple cheaters warning", "{count}, {names}", &GeneralSettings::m_MultipleCheaterWarningMessage)); v.back().m_Visible = CustomOnly;
				Add(Bool("party_warnings", section, "Party warnings for connecting cheaters", "Sends a party chat message when a marked player is joining.",
					&GeneralSettings::m_AutoChatWarningsConnectingParty));
				Add(Bool("party_summary", section, "Party warnings: print a summary on join", "The \"[TF2PL] Currently x players are marked...\" message.",
					&GeneralSettings::m_AutoChatWarningsConnectingPartyPrintSummary));
			}

			// --- Service integrations -------------------------------------------------------------------------------
			{
				const std::string section = "Service integrations";
				{
					auto& d = Add({ "allow_internet", section, "Allow internet connectivity", "Steam API, SteamHistory, player list updates and update checks need this.", SettingType::Bool });
					d.m_Get = [](const Settings& s) { return json(s.m_AllowInternetUsage.value_or(false)); };
					d.m_Set = [](Settings& s, const json& j) { s.m_AllowInternetUsage = ToBool(j); };
				}
				const auto Online = [](const Settings& s) { return s.m_AllowInternetUsage.value_or(false); };
				{
					auto& d = Add({ "steam_api_mode", section, "Steam API", "", SettingType::Enum });
					d.m_Options = { { "disabled", "Disabled" }, { "direct", "Enabled" } };
					d.m_Get = [](const Settings& s) { return json(s.GetSteamAPIMode() == SteamAPIMode::Direct ? "direct" : "disabled"); };
					d.m_Set = [](Settings& s, const json& j) { s.m_SteamAPIMode = ToString(j) == "direct" ? SteamAPIMode::Direct : SteamAPIMode::Disabled; };
					d.m_Visible = Online;
				}
				{
					auto& d = Add({ "steam_api_key", section, "Steam API key", "Get one at steamcommunity.com/dev/apikey. Never share it.", SettingType::Secret });
					d.m_Get = [](const Settings& s) { return json(!s.GetSteamAPIKey().empty()); };
					d.m_Set = [](Settings& s, const json& j) { s.SetSteamAPIKey(ToString(j, 64)); };
					d.m_Visible = [](const Settings& s) { return s.m_AllowInternetUsage.value_or(false) && s.GetSteamAPIMode() == SteamAPIMode::Direct; };
				}
				{
					auto& d = v.emplace_back(Bool("steam_history", section, "SteamHistory integration (SourceBans)", "", &GeneralSettings::m_EnableSteamHistoryIntegration));
					d.m_Visible = Online;
				}
				{
					auto& d = Add({ "steam_history_api_key", section, "SteamHistory API key", "Generate one at steamhistory.net/api.", SettingType::Secret });
					d.m_Get = [](const Settings& s) { return json(!s.GetSteamHistoryAPIKey().empty()); };
					d.m_Set = [](Settings& s, const json& j) { s.SetSteamHistoryAPIKey(ToString(j, 128)); };
					d.m_Visible = [](const Settings& s) { return s.m_AllowInternetUsage.value_or(false) && s.m_EnableSteamHistoryIntegration; };
				}
				{
					auto& d = Add({ "update_channel", section, "Automatic update checking", "", SettingType::Enum });
					d.m_Options = { { "none", "Disabled" }, { "public", "Releases" }, { "preview", "Preview" }, { "nightly", "Nightly" } };
					d.m_Get = [](const Settings& s)
					{
						switch (s.m_ReleaseChannel.value_or(ReleaseChannel::None))
						{
						case ReleaseChannel::Public:  return json("public");
						case ReleaseChannel::Preview: return json("preview");
						case ReleaseChannel::Nightly: return json("nightly");
						case ReleaseChannel::None:    return json("none");
						}
						return json("none");
					};
					d.m_Set = [](Settings& s, const json& j)
					{
						const auto value = ToString(j);
						if (value == "public") s.m_ReleaseChannel = ReleaseChannel::Public;
						else if (value == "preview") s.m_ReleaseChannel = ReleaseChannel::Preview;
						else if (value == "nightly") s.m_ReleaseChannel = ReleaseChannel::Nightly;
						else s.m_ReleaseChannel = ReleaseChannel::None;
					};
					d.m_Visible = Online;
				}
				{
					auto& d = Add({ "discord_rich_presence", section, "Discord integrations", "Discord Rich Presence (Windows builds with Discord support).", SettingType::Bool });
					d.m_Get = [](const Settings& s) { return json(s.m_Discord.m_EnableRichPresence); };
					d.m_Set = [](Settings& s, const json& j) { s.m_Discord.m_EnableRichPresence = ToBool(j); };
				}
			}

			// --- Interface ------------------------------------------------------------------------------------------
			{
				const std::string section = "Interface";
				{
					auto& d = Add({ "ui_mode", section, "Interface",
						"Native window (default) or this web page. The in-game overlay is only available with the native window.", SettingType::Enum });
					d.m_Options = { { "native", "Native window" }, { "web", "Web browser" } };
					d.m_Get = [](const Settings& s) { return json(s.IsWebUIMode() ? "web" : "native"); };
					d.m_Set = [](Settings& s, const json& j) { s.m_UIMode = ToString(j) == "web" ? TF2BDUIMode::Web : TF2BDUIMode::Native; };
				}
				{
					auto& d = Add({ "web_ui_port", section, "Web UI port",
						"Changing it restarts the web server: this page will move to http://127.0.0.1:<port>/.", SettingType::Int, 1024, 65535 });
					d.m_Get = [](const Settings& s) { return json(s.m_WebUIPort); };
					d.m_Set = [](Settings& s, const json& j) { s.m_WebUIPort = uint16_t(ToNumber(j, 1024, 65535)); };
				}
				Add(Bool("web_ui_open_browser", section, "Open the browser when TF2PL starts", "", &GeneralSettings::m_WebUIOpenBrowser));
				Add(Bool("kill_logs", section, "Show kill logs in chat", "", &GeneralSettings::m_KillLogsInChat));
				{
					const auto ViewBool = [&](const char* id, const char* label, bool Settings::UIState::MainWindow::* member)
					{
						auto& d = Add({ id, section, label, "", SettingType::Bool });
						d.m_Get = [member](const Settings& s) { return json(s.m_UIState.m_MainWindow.*member); };
						d.m_Set = [member](Settings& s, const json& j) { s.m_UIState.m_MainWindow.*member = ToBool(j); };
					};
					ViewBool("view.chat", "Show chat", &Settings::UIState::MainWindow::m_ChatEnabled);
					ViewBool("view.scoreboard", "Show scoreboard", &Settings::UIState::MainWindow::m_ScoreboardEnabled);
					ViewBool("view.team_stats", "Show team stats", &Settings::UIState::MainWindow::m_TeamStatsEnabled);
					ViewBool("view.app_log", "Show app log", &Settings::UIState::MainWindow::m_AppLogEnabled);
				}
			}

			// --- Colors ---------------------------------------------------------------------------------------------
			for (const auto& color : COLOR_SETTINGS)
			{
				auto& d = Add({ "color."s + color.m_Key, "Colors", color.m_Label, "", SettingType::Color });
				const ColorMember member = color.m_Member;
				d.m_Get = [member](const Settings& s) { return ColorToJson(s.m_Theme.m_Colors.*member); };
				d.m_Set = [member](Settings& s, const json& j) { s.m_Theme.m_Colors.*member = ColorFromJson(j); };
			}

			// --- Performance / logging / compatibility --------------------------------------------------------------
			{
				const std::string section = "Performance & logging";
				Add(Bool("sleep_when_unfocused", section, "Sleep when unfocused", "Slows TF2PL's refresh rate when its window isn't focused.", &GeneralSettings::m_SleepWhenUnfocused));
				Add(Bool("use_g15", section, "Use G15 player data (recommended)",
					"Reads players with TF2's g15_dumpplayer instead of polling status. Needs -g15 (added automatically when TF2PL launches TF2).",
					&GeneralSettings::m_UseG15PlayerData));
				Add(Bool("config_compat", section, "Config compatibility mode",
					"Improves compatibility with configs like mastercomfig (fewer \"too many commands\" disconnects) at the cost of slower scoreboard updates when joining.",
					&GeneralSettings::m_ConfigCompatibilityMode));
				Add(Bool("save_console_logs", section, "Log console entries", "", &GeneralSettings::m_SaveConsoleLogs));
				Add(Bool("save_chat_history", section, "Log chat history", "", &GeneralSettings::m_SaveChatHistory));
				{
					auto& d = Add({ "log_rcon", section, "Log RCON packets", "", SettingType::Bool });
					d.m_Get = [](const Settings& s) { return json(s.m_Logging.m_RCONPackets); };
					d.m_Set = [](Settings& s, const json& j) { s.m_Logging.m_RCONPackets = ToBool(j); };
				}
				{
					auto& d = Add({ "log_discord", section, "Log Discord Rich Presence", "", SettingType::Bool });
					d.m_Get = [](const Settings& s) { return json(s.m_Logging.m_DiscordRichPresence); };
					d.m_Set = [](Settings& s, const json& j) { s.m_Logging.m_DiscordRichPresence = ToBool(j); };
				}
			}

			// --- Game -----------------------------------------------------------------------------------------------
			{
				const std::string section = "Game";
				Add(Bool("auto_launch_tf2", section, "Launch TF2 when TF2PL starts", "", &GeneralSettings::m_AutoLaunchTF2));
				Add(Bool("quit_when_tf_closes", section, "Quit TF2PL when TF2 closes", "", &GeneralSettings::m_ShouldCloseWhenTFCloses));
				{
					auto& d = Add({ "tf_binary_mode", section, "TF2 binary", "", SettingType::Enum });
					d.m_Options = { { "x64", "64-bit" }, { "x86", "32-bit" }, { "x86_legacy", "32-bit (legacy hl2)" } };
					d.m_Get = [](const Settings& s)
					{
						switch (s.m_TFBinaryMode)
						{
						case TFBinaryMode::x64:        return json("x64");
						case TFBinaryMode::x86:        return json("x86");
						case TFBinaryMode::x86_legacy: return json("x86_legacy");
						}
						return json("x64");
					};
					d.m_Set = [](Settings& s, const json& j)
					{
						const auto value = ToString(j);
						s.m_TFBinaryMode = value == "x86" ? TFBinaryMode::x86 : value == "x86_legacy" ? TFBinaryMode::x86_legacy : TFBinaryMode::x64;
					};
				}
				Add(Bool("use_recommended_launch_params", section, "Use recommended launch options", "", &GeneralSettings::m_UseLaunchRecommendedParams));
				Add(Bool("rcon_static", section, "Use static RCON launch parameters (not recommended)", "", &GeneralSettings::m_UseRconStaticParams));
				{
					auto& d = v.emplace_back(Text("rcon_static_password", section, "Static RCON password", "", &GeneralSettings::m_RconStaticPassword));
					d.m_Visible = [](const Settings& s) { return s.m_UseRconStaticParams; };
				}
				{
					auto& d = Add({ "rcon_static_port", section, "Static RCON port", "", SettingType::Int, 1024, 65535 });
					d.m_Get = [](const Settings& s) { return json(s.m_RconStaticPort); };
					d.m_Set = [](Settings& s, const json& j) { s.m_RconStaticPort = uint16_t(ToNumber(j, 1024, 65535)); };
					d.m_Visible = [](const Settings& s) { return s.m_UseRconStaticParams; };
				}
			}

			// --- Autodetected overrides -----------------------------------------------------------------------------
			{
				const std::string section = "Autodetected overrides";
				{
					auto& d = Add({ "steam_dir_override", section, "Steam directory", "Leave empty to detect it automatically.", SettingType::String });
					d.m_Get = [](const Settings& s) { return json(FromPath(s.m_SteamDirOverride)); };
					d.m_Set = [](Settings& s, const json& j) { s.m_SteamDirOverride = ToPath(ToString(j)); };
					d.m_Placeholder = [](const Settings& s) { return "auto: " + FromPath(s.GetSteamDir()); };
				}
				{
					auto& d = Add({ "tf_dir_override", section, "tf directory", "Leave empty to detect it automatically.", SettingType::String });
					d.m_Get = [](const Settings& s) { return json(FromPath(s.m_TFDirOverride)); };
					d.m_Set = [](Settings& s, const json& j) { s.m_TFDirOverride = ToPath(ToString(j)); };
					d.m_Placeholder = [](const Settings& s) { return "auto: " + FromPath(s.GetTFDir()); };
				}
				{
					auto& d = Add({ "local_steamid_override", section, "My Steam ID",
						"Leave empty to use the account Steam is signed in to. Hidden while streamer mode is on.", SettingType::String });
					d.m_Get = [](const Settings& s)
					{
						if (s.m_StreamerMode.m_Enabled)
							return json("");
						return json(s.m_LocalSteamIDOverride.IsValid() ? s.m_LocalSteamIDOverride.str() : std::string{});
					};
					d.m_Set = [](Settings& s, const json& j)
					{
						const auto text = ToString(j, 128);
						if (text.empty())
						{
							s.m_LocalSteamIDOverride = {};
							return;
						}
						auto id = WebUIData::ParseSteamID(text);
						if (!id)
							throw std::invalid_argument("not a valid SteamID");
						s.m_LocalSteamIDOverride = *id;
					};
					d.m_Visible = [](const Settings& s) { return !s.m_StreamerMode.m_Enabled; };
				}
			}

			return v;
		}();
		return s_Descs;
	}
}

json WebUIData::BuildSettings(TF2BDApplication& app)
{
	const auto& settings = app.GetSettings();

	json sections = json::array();
	std::map<std::string, size_t> sectionIndex;
	const auto Section = [&](const std::string& name) -> json&
	{
		if (auto found = sectionIndex.find(name); found != sectionIndex.end())
			return sections[found->second];
		sectionIndex[name] = sections.size();
		sections.push_back({ { "name", name }, { "items", json::array() } });
		return sections.back();
	};

	for (const auto& desc : GetSettingDescs())
	{
		json item =
		{
			{ "id", desc.m_ID },
			{ "label", desc.m_Label },
			{ "help", desc.m_Help },
			{ "type", SettingTypeName(desc.m_Type) },
			{ "value", desc.m_Get(settings) },
			{ "visible", !desc.m_Visible || desc.m_Visible(settings) },
		};
		if (desc.m_Type == SettingType::Int || desc.m_Type == SettingType::Float)
		{
			item["min"] = desc.m_Min;
			item["max"] = desc.m_Max;
			item["step"] = desc.m_Step;
		}
		if (!desc.m_Options.empty())
		{
			json options = json::array();
			for (const auto& [value, label] : desc.m_Options)
				options.push_back({ { "value", value }, { "label", label } });
			item["options"] = std::move(options);
		}
		if (desc.m_Placeholder)
			item["placeholder"] = desc.m_Placeholder(settings);

		Section(desc.m_Section)["items"].push_back(std::move(item));
	}

	// mods/addons are files on disk, so they're listed live
	{
		auto& mods = Section("Mods");
		IAddonManager& addons = IAddonManager::Get();
		for (const std::filesystem::path& path : addons.GetAllAvailableAddons())
		{
			const std::string file = FromPath(path.filename());
			mods["items"].push_back({
				{ "id", "addon:" + file },
				{ "label", file },
				{ "help", "TF2 and TF2PL must be restarted to apply changes to mods." },
				{ "type", "bool" },
				{ "value", addons.IsAddonEnabled(settings, path) },
				{ "visible", true },
			});
		}
	}

	return { { "sections", std::move(sections) },
		{ "nativeOnly", "Overlay, graphics (renderer/GPU), font and UI scale settings only apply to the native window, so they're only in its Settings." } };
}

void WebUIData::ApplySetting(TF2BDApplication& app, const std::string& id, const json& value)
{
	auto& settings = app.GetSettings();

	if (id.starts_with("addon:"))
	{
		const std::string file = id.substr(6);
		IAddonManager& addons = IAddonManager::Get();
		for (const std::filesystem::path& path : addons.GetAllAvailableAddons())
		{
			if (FromPath(path.filename()) == file)
			{
				addons.SetAddonEnabled(settings, path, ToBool(value));
				settings.SaveFile();
				return;
			}
		}
		throw std::invalid_argument("unknown mod");
	}

	for (const auto& desc : GetSettingDescs())
	{
		if (desc.m_ID != id)
			continue;

		if (desc.m_Visible && !desc.m_Visible(settings))
			throw std::invalid_argument("that setting is not available right now");

		desc.m_Set(settings, value);
		settings.SaveFile();
		Log("Web UI: changed setting {}", desc.m_Label);
		return;
	}

	throw std::invalid_argument("unknown setting");
}
