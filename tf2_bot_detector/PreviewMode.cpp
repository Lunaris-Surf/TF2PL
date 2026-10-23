#include "PreviewMode.h"
#include "Log.h"
#include "ModeratorLogic.h"
#include "WorldState.h"
#include "Config/Settings.h"
#include "ConsoleLog/ConsoleLines/ChatConsoleLine.h"
#include "GameData/IPlayer.h"
#include "Networking/LogsTFAPI.h"
#include "Networking/SteamAPI.h"
#include "Networking/SteamHistoryAPI.h"

#include <array>
#include <set>

using namespace std::chrono_literals;
using namespace tf2_bot_detector;
using namespace tf2_bot_detector::Testing;

namespace
{
	constexpr uint32_t SAMPLE_PLAYER_COUNT = 12;
	constexpr uint32_t CONNECTING_PLAYER = 11;

	struct ScriptedChat
	{
		uint32_t m_Player;
		const char* m_Message;
		bool m_Team;
		bool m_Dead;
	};

	// player indices refer to PopulateSampleLobby: 0 = you, 1 = your friend, 0-5 RED, 6-10 BLU
	constexpr std::array<ScriptedChat, 16> CHAT_SCRIPT =
	{ {
		{ 2, "gg", false, false },
		{ 4, "ez", false, false },
		{ 3, "uber ready", true, false },
		{ 1, "push the cart, they're all on the left", true, false },
		{ 7, "why does everyone keep kicking me", false, true },
		{ 8, "nice try spy", false, false },
		{ 0, "anyone got a spare sandvich?", false, false },
		{ 5, "sandvich is for me", false, false },
		{ 9, "lol", false, true },
		{ 6, "dispenser here", false, false },
		{ 3, "pls stop running past me", true, false },
		{ 4, "you're all bad", false, true },
		{ 2, "mmph mmph", false, false },
		{ 10, "how is that guy hitting every shot", false, false },
		{ 0, "kicking the sniper, vote f1", false, false },
		{ 1, "voted", true, false },
	} };
}

namespace tf2_bot_detector
{
	/// see PreviewSession; a friend of it so it can read the sample marks and world
	class PreviewModeratorLogic final : public IModeratorLogic
	{
	public:
		PreviewModeratorLogic(PreviewSession& session, IModeratorLogic& real) :
			m_Session(session), m_Real(real)
		{
		}

		void Update() override {}

		bool InitiateVotekick(const IPlayer& player, KickReason, const PlayerMarks*) override
		{
			Log("Preview mode: not calling a votekick on {} (game actions are disabled while previewing)", player.GetNameSafe());
			return false;
		}

		PlayerMarks GetPlayerAttributes(const SteamID& id) const override
		{
			if (!m_Session.OwnsPlayer(id))
				return m_Real.GetPlayerAttributes(id);

			PlayerMarks marks;
			if (auto found = m_Session.m_Marks.find(id); found != m_Session.m_Marks.end() && !found->second.empty())
				marks.m_Marks.emplace_back(found->second, ConfigFileName(PreviewSession::LIST_NAME));
			return marks;
		}

		PlayerMarks HasPlayerAttributes(const SteamID& id, const PlayerAttributesList& attributes,
			AttributePersistence persistence) const override
		{
			if (!m_Session.OwnsPlayer(id))
				return m_Real.HasPlayerAttributes(id, attributes, persistence);

			PlayerMarks marks;
			if (auto found = m_Session.m_Marks.find(id); found != m_Session.m_Marks.end())
			{
				if (auto matching = found->second & attributes; !matching.empty())
					marks.m_Marks.emplace_back(matching, ConfigFileName(PreviewSession::LIST_NAME));
			}
			return marks;
		}

		bool SetPlayerAttribute(const IPlayer& player, PlayerAttribute markType, AttributePersistence persistence,
			bool set, std::string proof) override
		{
			return SetPlayerAttribute(player.GetSteamID(), player.GetNameUnsafe(), markType, persistence, set, std::move(proof));
		}

		bool SetPlayerAttribute(const SteamID& id, std::string name, PlayerAttribute markType, AttributePersistence persistence,
			bool set, std::string proof) override
		{
			if (!m_Session.OwnsPlayer(id))
				return m_Real.SetPlayerAttribute(id, std::move(name), markType, persistence, set, std::move(proof));

			auto& attrs = m_Session.m_Marks[id];
			if (attrs.HasAttribute(markType) == set)
				return false;

			attrs.SetAttribute(markType, set);
			Log("Preview mode: {} {} {:v} (sample player, not saved)", name, set ? "marked" : "unmarked", tf2pl::enum_fmt(markType));
			return true;
		}

		bool SetPlayerCustomTag(const SteamID& id, const std::string& name, const std::string& tag, AttributePersistence persistence,
			bool set, const std::string& proof) override
		{
			if (!m_Session.OwnsPlayer(id))
				return m_Real.SetPlayerCustomTag(id, name, tag, persistence, set, proof);

			if (!PlayerAttributesList::IsValidCustomTag(tag))
				return false;

			auto& attrs = m_Session.m_Marks[id];
			if (attrs.GetCustomTags().contains(tag) == set)
				return false;

			attrs.SetCustomTag(tag, set);
			Log("Preview mode: {} tag {} on {} (sample player, not saved)", set ? "added" : "removed", tag.substr(7), name);
			return true;
		}

		TeamShareResult GetTeamShareResult(const SteamID& id) const override
		{
			return m_Session.m_World.GetTeamShareResult(id);
		}

		const IPlayer* GetBotLeader() const override { return nullptr; }

		bool IsUserRunningTool(const SteamID& id) const override { return m_RunningTool.contains(id); }
		void SetUserRunningTool(const SteamID& id, bool isUserRunningTool) override
		{
			if (isUserRunningTool)
				m_RunningTool.insert(id);
			else
				m_RunningTool.erase(id);
		}

		size_t GetBlacklistedPlayerCount() const override { return m_Real.GetBlacklistedPlayerCount(); }
		size_t GetRuleCount() const override { return m_Real.GetRuleCount(); }

		MarkedFriends GetMarkedFriendsCount(IPlayer& player) const override
		{
			MarkedFriends result;
			const auto& friends = player.GetFriendsInfo();
			if (!friends.has_value())
				return result;

			result.m_FriendsCountTotal = static_cast<uint32_t>(friends->m_Friends.size());
			for (const SteamID& friendID : friends->m_Friends)
			{
				const auto marks = GetPlayerAttributes(friendID);
				if (marks.empty())
					continue;

				result.m_MarkedFriendsCountTotal++;

				PlayerAttributesList combined;
				for (const auto& mark : marks)
					combined |= mark.m_Attributes;
				for (size_t i = 0; i < PlayerAttributesList::size(); i++)
				{
					if (combined.HasAttribute(PlayerAttribute(i)))
						result.m_MarkedFriendsCount[PlayerAttribute(i)]++;
				}
			}
			return result;
		}

		void ReloadConfigFiles() override { m_Real.ReloadConfigFiles(); }
		PlayerListJSON* GetPlayerList() override { return m_Real.GetPlayerList(); }

	private:
		PreviewSession& m_Session;
		IModeratorLogic& m_Real;
		std::set<SteamID> m_RunningTool;
	};
}

PreviewSession::PreviewSession(IModeratorLogic& realModLogic, const Settings& settings) :
	m_Settings(settings)
{
	// the sample lobby lives in "now", so connection times and account ages look right
	m_World.m_CurrentTime = tfbd_clock_t::now();
	m_World.m_LastStatusUpdateTime = m_World.m_CurrentTime;
	PopulatePreviewLobby(m_World, settings.GetLocalSteamID());

	m_ModLogic = std::make_unique<PreviewModeratorLogic>(*this, realModLogic);

	// a few marks so the scoreboard, overlay and marked friends have something to show
	const auto Mark = [&](uint32_t index, std::initializer_list<PlayerAttribute> attrs, const char* tag = nullptr)
	{
		auto& list = m_Marks[FakeSteamID(index)];
		for (auto attr : attrs)
			list.SetAttribute(attr);
		if (tag)
			list.SetCustomTag(tag);
	};
	Mark(4, { PlayerAttribute::Cheater, PlayerAttribute::VACBanned });
	Mark(7, { PlayerAttribute::Cheater });
	Mark(8, { PlayerAttribute::Suspicious });
	Mark(9, { PlayerAttribute::Exploiter });
	Mark(2, {}, "custom:watchlist");

	for (size_t i = 0; i < 6; i++)
	{
		const auto& chat = CHAT_SCRIPT[i];
		AddChatLine(FakeSteamID(chat.m_Player), chat.m_Message, chat.m_Team, chat.m_Dead);
	}
	m_ChatIndex = 6;

	m_LastTick = m_LastChat = tfbd_clock_t::now();
	Log("Preview mode started: showing a sample lobby. Nothing is sent to TF2 and sample marks aren't saved.");
}

PreviewSession::~PreviewSession()
{
	Log("Preview mode ended.");
}

IWorldState& PreviewSession::GetWorld() { return m_World; }
const IWorldState& PreviewSession::GetWorld() const { return m_World; }
IModeratorLogic& PreviewSession::GetModLogic() { return *m_ModLogic; }
const IModeratorLogic& PreviewSession::GetModLogic() const { return *m_ModLogic; }

bool PreviewSession::OwnsPlayer(const SteamID& id) const
{
	return m_World.FindPlayer(id) != nullptr;
}

PlayerAttributesList PreviewSession::GetEditableAttributes(const SteamID& id) const
{
	if (auto found = m_Marks.find(id); found != m_Marks.end())
		return found->second;
	return {};
}

void PreviewSession::AddChatLine(const SteamID& id, std::string_view message, bool team, bool dead)
{
	const auto* player = m_World.FindPlayer(id);
	if (!player)
		return;

	m_Lines.push_front(std::make_shared<ChatConsoleLine>(m_World.GetCurrentTime(), player->GetNameUnsafe(),
		std::string(message), dead, team, id == m_World.m_LocalSteamID, m_World.GetTeamShareResult(id), id));

	while (m_Lines.size() > 200)
		m_Lines.pop_back();
}

void PreviewSession::Update()
{
	const auto now = tfbd_clock_t::now();
	m_World.m_CurrentTime = now;
	m_World.m_LastStatusUpdateTime = now;

	if (now - m_LastTick >= 2s)
	{
		m_LastTick = now;
		m_TickCount++;

		std::vector<FakePlayer*> active;
		for (uint32_t i = 0; i < SAMPLE_PLAYER_COUNT; i++)
		{
			if (i == CONNECTING_PLAYER)
				continue;

			// player 0 may have been replaced by your own SteamID
			FakePlayer* player = m_World.FindFakePlayer(FakeSteamID(i));
			if (!player && i == 0)
				player = m_World.FindFakePlayer(m_World.m_LocalSteamID);
			if (!player)
				continue;

			player->m_LastStatusUpdateTime = now;
			const int jitter = int((m_TickCount * 7 + i * 3) % 9) - 4;
			player->m_Ping = uint16_t(std::max(5, int(30 + i * 7) + jitter));
			active.push_back(player);
		}

		// someone scores every few seconds
		if (m_TickCount % 2 == 0 && active.size() > 6)
		{
			FakePlayer* killer = active[(m_TickCount * 5) % active.size()];
			FakePlayer* victim = nullptr;
			for (size_t offset = 1; offset < active.size(); offset++)
			{
				FakePlayer* candidate = active[(m_TickCount * 3 + offset) % active.size()];
				if (candidate != killer && candidate->m_Team != killer->m_Team)
				{
					victim = candidate;
					break;
				}
			}
			if (victim)
			{
				killer->m_Scores.m_Kills++;
				victim->m_Scores.m_Deaths++;
			}
		}
	}

	if (now - m_LastChat >= 7s)
	{
		m_LastChat = now;
		const auto& chat = CHAT_SCRIPT[m_ChatIndex % CHAT_SCRIPT.size()];
		const SteamID id = chat.m_Player == 0 ? m_World.m_LocalSteamID : FakeSteamID(chat.m_Player);
		AddChatLine(id, chat.m_Message, chat.m_Team, chat.m_Dead);
		m_ChatIndex++;
	}
}

void tf2_bot_detector::PopulatePreviewLobby(FakeWorldState& world, const SteamID& localID)
{
	PopulateSampleLobby(world);

	world.m_ConsoleOutput.clear();
	world.m_ServerHostName = "TF2PL Preview // sample lobby";
	world.m_MapName = "pl_upward";
	world.m_VoteInProgress = false;

	const auto Days = [](int days) { return duration_t(std::chrono::hours(24 * days)); };
	const auto loading = make_error_condition(std::errc::operation_in_progress);

	for (uint32_t i = 0; i < SAMPLE_PLAYER_COUNT; i++)
	{
		FakePlayer* p = world.FindFakePlayer(FakeSteamID(i));
		if (!p)
			continue;

		if (i == CONNECTING_PLAYER)
		{
			// still connecting: everything is "loading"
			p->m_Summary = loading;
			p->m_Bans = loading;
			p->m_SourceBanState = loading;
			p->m_SourceBans = loading;
			p->m_TF2Playtime = loading;
			p->m_Logs = loading;
			p->m_Friends = loading;
			p->m_Inventory = loading;
			continue;
		}

		SteamAPI::PlayerSummary summary = p->m_Summary.value();
		summary.m_Visibility = SteamAPI::CommunityVisibilityState::Public;
		summary.m_ProfileConfigured = true;
		summary.m_CreationTime = world.m_CurrentTime - Days(365 * int(1 + i % 8) + 37 * int(i));
		p->m_Summary = summary;

		p->m_Logs = LogsTFAPI::PlayerLogsInfo{ p->m_SteamID, 12 + i * 31 };
		p->m_Inventory = SteamAPI::PlayerInventoryInfo{ 40 + i * 13, 300 };
		p->m_SourceBanState = SteamHistoryAPI::PlayerSourceBanState{};
		p->m_SourceBans = SteamHistoryAPI::PlayerSourceBans{};
		if (!p->m_Friends.has_value() && p->m_Friends.error() == NotLoadedError())
			p->m_Friends = SteamAPI::PlayerFriends{};
	}

	// the obvious cheater: new account, VAC ban, a SourceBans record
	if (FakePlayer* p = world.FindFakePlayer(FakeSteamID(4)))
	{
		auto summary = p->m_Summary.value();
		summary.m_CreationTime = world.m_CurrentTime - Days(41);
		p->m_Summary = summary;

		SteamAPI::PlayerBans bans = p->m_Bans.value();
		bans.m_VACBanCount = 1;
		bans.m_TimeSinceLastBan = Days(400);
		p->m_Bans = bans;

		p->m_TF2Playtime = duration_t(std::chrono::hours(61));
		p->m_Inventory = SteamAPI::PlayerInventoryInfo{ 3, 50 };

		SteamHistoryAPI::PlayerSourceBan ban;
		ban.m_ID = p->m_SteamID;
		ban.m_BanState = SteamHistoryAPI::Permanent;
		ban.m_UserName = p->m_Name;
		ban.m_BanReason = "Cheating (aimbot)";
		ban.m_BanTimestamp = world.m_CurrentTime - Days(90);
		ban.m_Server = "Sample Community";
		p->m_SourceBanState = SteamHistoryAPI::PlayerSourceBanState{ { ban.m_Server, ban } };
		p->m_SourceBans = SteamHistoryAPI::PlayerSourceBans{ ban };
	}

	// another cheater with a game ban
	if (FakePlayer* p = world.FindFakePlayer(FakeSteamID(7)))
	{
		SteamAPI::PlayerBans bans = p->m_Bans.value();
		bans.m_GameBanCount = 1;
		bans.m_TimeSinceLastBan = Days(120);
		p->m_Bans = bans;
	}

	// private profile
	if (FakePlayer* p = world.FindFakePlayer(FakeSteamID(10)))
	{
		auto summary = p->m_Summary.value();
		summary.m_Visibility = SteamAPI::CommunityVisibilityState::Private;
		summary.m_ProfileConfigured = false;
		summary.m_CreationTime.reset();
		p->m_Summary = summary;
		p->m_TF2Playtime = std::error_condition(SteamAPI::ErrorCode::InfoPrivate);
		p->m_Inventory = std::error_condition(SteamAPI::ErrorCode::InfoPrivate);
	}

	// player 0 is you
	if (localID.IsValid() && localID != FakeSteamID(0))
	{
		if (FakePlayer* p = world.FindFakePlayer(FakeSteamID(0)))
		{
			p->m_SteamID = localID;
			p->m_Name = "You";
			if (p->m_LobbyMember)
				p->m_LobbyMember->m_SteamID = localID;
			if (p->m_Summary.has_value())
			{
				auto summary = p->m_Summary.value();
				summary.m_SteamID = localID;
				summary.m_Nickname = p->m_Name;
				p->m_Summary = summary;
			}
			if (p->m_Bans.has_value())
			{
				auto bans = p->m_Bans.value();
				bans.m_SteamID = localID;
				p->m_Bans = bans;
			}
			world.SetLocalPlayer(localID);
		}
	}
	else if (FakePlayer* p = world.FindFakePlayer(FakeSteamID(0)))
	{
		p->m_Name = "You";
		if (p->m_Summary.has_value())
		{
			auto summary = p->m_Summary.value();
			summary.m_Nickname = p->m_Name;
			p->m_Summary = summary;
		}
	}
}
