#pragma once

// Reusable fake world/application state for tests and deterministic UI work.
//
// FakeWorldState implements IWorldState entirely in memory: no TF2, no console.log, no Steam session
// and no network. Players are FakePlayer objects whose every field is a plain public member, so a test
// (or a demo/preview mode) can set up exactly the lobby it needs and get the same result every time.
// PopulateSampleLobby() builds a representative 12-player match (teams, local player, a friend,
// friends lists, a vote, chat lines) to start from.
//
// Player-list *marks* live in ModeratorLogic/PlayerListJSON, not the world state; pair this with a
// PlayerListJSON loaded from test data when marks are needed.

#include "WorldState.h"
#include "LobbyMember.h"
#include "PlayerStatus.h"
#include "SteamID.h"
#include "Config/AccountAges.h"
#include "ConsoleLog/ConsoleLineListener.h"
#include "GameData/IPlayer.h"
#include "GameData/TFConstants.h"
#include "Networking/LogsTFAPI.h"
#include "Networking/SteamAPI.h"
#include "Networking/SteamHistoryAPI.h"

#include <any>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace tf2_bot_detector::Testing
{
	class FakeWorldState;

	/// "Not fetched yet" error used as the default for every optional Steam/logs.tf lookup.
	std::error_condition NotLoadedError();

	class FakePlayer final : public IPlayer
	{
	public:
		FakePlayer(FakeWorldState& world, SteamID id, std::string name);

		// --- freely editable state ---
		std::string m_Name;
		SteamID m_SteamID;
		TFTeam m_Team = TFTeam::Unknown;
		std::optional<LobbyMember> m_LobbyMember;
		std::optional<UserID_t> m_UserID;
		PlayerStatusState m_State = PlayerStatusState::Active;
		PlayerScores m_Scores;
		uint16_t m_Ping = 50;
		bool m_IsFriend = false;
		time_point_t m_ConnectionTime{};
		time_point_t m_LastStatusUpdateTime{};
		duration_t m_ActiveTime{};
		std::optional<time_point_t> m_EstimatedAccountCreationTime;

		tf2pl::expected<SteamAPI::PlayerSummary> m_Summary = NotLoadedError();
		tf2pl::expected<SteamAPI::PlayerBans> m_Bans = NotLoadedError();
		tf2pl::expected<SteamHistoryAPI::PlayerSourceBanState> m_SourceBanState = NotLoadedError();
		tf2pl::expected<SteamHistoryAPI::PlayerSourceBans> m_SourceBans = NotLoadedError();
		tf2pl::expected<duration_t> m_TF2Playtime = NotLoadedError();
		tf2pl::expected<LogsTFAPI::PlayerLogsInfo> m_Logs = NotLoadedError();
		tf2pl::expected<SteamAPI::PlayerFriends> m_Friends = NotLoadedError();
		tf2pl::expected<SteamAPI::PlayerInventoryInfo> m_Inventory = NotLoadedError();

		/// Adds a friend (by SteamID) to this player's friends list, creating the list if it was "not loaded".
		FakePlayer& AddFriend(const SteamID& id);

		// --- IPlayer ---
		using IPlayer::GetWorld;
		const IWorldState& GetWorld() const override;
		const LobbyMember* GetLobbyMember() const override { return m_LobbyMember ? &*m_LobbyMember : nullptr; }
		std::string GetNameUnsafe() const override { return m_Name; }
		SteamID GetSteamID() const override { return m_SteamID; }
		const tf2pl::expected<SteamAPI::PlayerSummary>& GetPlayerSummary() const override { return m_Summary; }
		const tf2pl::expected<SteamAPI::PlayerBans>& GetPlayerBans() const override { return m_Bans; }
		const tf2pl::expected<SteamHistoryAPI::PlayerSourceBanState>& GetPlayerSourceBanState() const override { return m_SourceBanState; }
		const tf2pl::expected<SteamHistoryAPI::PlayerSourceBans>& GetPlayerSourceBans() const override { return m_SourceBans; }
		tf2pl::expected<duration_t> GetTF2Playtime() const override { return m_TF2Playtime; }
		bool IsFriend() const override { return m_IsFriend; }
		std::optional<UserID_t> GetUserID() const override { return m_UserID; }
		PlayerStatusState GetConnectionState() const override { return m_State; }
		time_point_t GetConnectionTime() const override { return m_ConnectionTime; }
		duration_t GetConnectedTime() const override;
		TFTeam GetTeam() const override { return m_Team; }
		const PlayerScores& GetScores() const override { return m_Scores; }
		uint16_t GetPing() const override { return m_Ping; }
		time_point_t GetLastStatusUpdateTime() const override { return m_LastStatusUpdateTime; }
		std::optional<time_point_t> GetEstimatedAccountCreationTime() const override { return m_EstimatedAccountCreationTime; }
		const tf2pl::expected<LogsTFAPI::PlayerLogsInfo>& GetLogsInfo() const override { return m_Logs; }
		const tf2pl::expected<SteamAPI::PlayerFriends>& GetFriendsInfo() const override { return m_Friends; }
		const tf2pl::expected<SteamAPI::PlayerInventoryInfo>& GetInventoryInfo() const override { return m_Inventory; }
		duration_t GetActiveTime() const override { return m_ActiveTime; }
		std::any& GetOrCreateDataStorage(const std::type_index& type) override { return m_Data[type]; }

	protected:
		const std::any* FindDataStorage(const std::type_index& type) const override;

	private:
		FakeWorldState* m_World = nullptr;
		std::map<std::type_index, std::any> m_Data;
	};

	class FakeAccountAges final : public IAccountAges
	{
	public:
		void OnDataReady(const SteamID& id, time_point_t creationTime) override { m_Ages[id] = creationTime; }
		std::optional<time_point_t> EstimateAccountCreationTime(const SteamID& id) const override;

		std::unordered_map<SteamID, time_point_t> m_Ages;
	};

	class FakeWorldState final : public IWorldState
	{
	public:
		FakeWorldState();

		// --- setup helpers ---
		FakePlayer& AddPlayer(SteamID id, std::string name, TFTeam team = TFTeam::Unknown);
		FakePlayer* FindFakePlayer(const SteamID& id);
		void RemovePlayer(const SteamID& id);
		void ClearPlayers();
		void SetLocalPlayer(const SteamID& id) { m_LocalSteamID = id; }
		void AdvanceTime(duration_t amount) { m_CurrentTime += amount; }

		// --- freely editable state ---
		time_point_t m_CurrentTime{};
		time_point_t m_LastStatusUpdateTime{};
		SteamID m_LocalSteamID;
		bool m_VoteInProgress = false;
		std::string m_ServerHostName = "Valve Matchmaking Server (Virginia srcds1001-iad1 #12)";
		std::string m_MapName = "pl_upward";
		FakeAccountAges m_AccountAges;

		// Everything passed to AddConsoleOutputChunk/AddConsoleOutputLine, one entry per line.
		std::vector<std::string> m_ConsoleOutput;
		size_t m_G15DumpsApplied = 0;
		size_t m_UpdateCount = 0;
		size_t m_ScoreboardResets = 0;

		// --- IWorldStateConLog ---
		IConsoleLineListener& GetConsoleLineListenerBroadcaster() override { return m_Broadcaster; }
		void UpdateTimestamp(const ConsoleLogParser&) override {}

		// --- IWorldState ---
		void Update() override { m_UpdateCount++; }
		void ResetScoreboard() override;

		time_point_t GetCurrentTime() const override { return m_CurrentTime; }
		time_point_t GetLastStatusUpdateTime() const override { return m_LastStatusUpdateTime; }

		void AddWorldEventListener(IWorldEventListener* listener) override;
		void RemoveWorldEventListener(IWorldEventListener* listener) override;
		void AddConsoleLineListener(IConsoleLineListener* listener) override;
		void RemoveConsoleLineListener(IConsoleLineListener* listener) override;

		void AddConsoleOutputChunk(const std::string_view& chunk) override;
		tf2pl::task<> AddConsoleOutputLine(std::string line) override;

		bool ApplyG15Dump(const std::vector<G15PlayerData>& players) override;

		std::optional<SteamID> FindSteamIDForName(const std::string_view& playerName) const override;
		std::optional<LobbyMemberTeam> FindLobbyMemberTeam(const SteamID& id) const override;
		std::optional<UserID_t> FindUserID(const SteamID& id) const override;

		TeamShareResult GetTeamShareResult(const SteamID& id) const override;
		TeamShareResult GetTeamShareResult(const SteamID& id0, const SteamID& id1) const override;
		TeamShareResult GetTeamShareResult(const std::optional<LobbyMemberTeam>& team0, const SteamID& id1) const override;

		const IPlayer* FindPlayer(const SteamID& id) const override;
		const IPlayer* LocalPlayer() const override { return FindPlayer(m_LocalSteamID); }

		size_t GetApproxLobbyMemberCount() const override;
		tf2pl::generator<const IPlayer&> GetLobbyMembers() const override;
		tf2pl::generator<const IPlayer&> GetPlayers() const override;

		bool IsLocalPlayerInitialized() const override { return m_LocalSteamID.IsValid(); }
		bool IsVoteInProgress() const override { return m_VoteInProgress; }

		const std::string& GetServerHostName() const override { return m_ServerHostName; }
		const std::string& GetMapName() const override { return m_MapName; }

		const IAccountAges& GetAccountAges() const override { return m_AccountAges; }

		const std::vector<IWorldEventListener*>& GetWorldEventListeners() const { return m_WorldEventListeners; }
		const std::vector<IConsoleLineListener*>& GetConsoleLineListeners() const { return m_ConsoleLineListeners; }

	private:
		static TeamShareResult CompareTeams(const std::optional<LobbyMemberTeam>& team0, const std::optional<LobbyMemberTeam>& team1);

		struct Broadcaster final : IConsoleLineListener
		{
			explicit Broadcaster(FakeWorldState& world) : m_World(world) {}
			void OnConsoleLineParsed(IWorldState& world, IConsoleLine& line) override;
			void OnConsoleLineUnparsed(IWorldState& world, const std::string_view& text) override;
			void OnConsoleLogChunkParsed(IWorldState& world, bool consoleLinesParsed) override;
			FakeWorldState& m_World;
		} m_Broadcaster{ *this };

		std::vector<std::shared_ptr<FakePlayer>> m_Players; // join order
		std::vector<IWorldEventListener*> m_WorldEventListeners;
		std::vector<IConsoleLineListener*> m_ConsoleLineListeners;
	};

	/// SteamID for a fake individual account (account id = 1000 + index), stable across runs.
	SteamID FakeSteamID(uint32_t index);

	/// Fills the world with a representative casual match:
	///  - 12 players, 6 RED (invaders) / 6 BLU (defenders), each with a lobby member, user id, scores and ping
	///  - player 0 is the local player (RED); player 1 is a Steam friend of the local player (RED)
	///  - player 7 (BLU) has a public friends list containing players 8 and 9; player 10 has a private one
	///  - player 11 is still connecting and has no Steam data yet
	///  - a vote in progress and a few chat lines in the console output
	void PopulateSampleLobby(FakeWorldState& world);
}
