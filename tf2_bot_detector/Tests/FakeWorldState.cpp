#include "FakeWorldState.h"
#include "GenericErrors.h"

#include <algorithm>
#include <array>

using namespace std::chrono_literals;
using namespace tf2_bot_detector;
using namespace tf2_bot_detector::Testing;

std::error_condition tf2_bot_detector::Testing::NotLoadedError()
{
	return make_error_condition(ErrorCode::LazyValueUninitialized);
}

SteamID tf2_bot_detector::Testing::FakeSteamID(uint32_t index)
{
	return SteamID(1000 + index, SteamAccountType::Individual, SteamAccountUniverse::Public);
}

// ---------------------------------------------------------------------------------------------------------------------
// FakePlayer

FakePlayer::FakePlayer(FakeWorldState& world, SteamID id, std::string name) :
	m_Name(std::move(name)), m_SteamID(id), m_World(&world)
{
	m_ConnectionTime = world.GetCurrentTime();
	m_LastStatusUpdateTime = world.GetCurrentTime();
}

const IWorldState& FakePlayer::GetWorld() const
{
	return *m_World;
}

duration_t FakePlayer::GetConnectedTime() const
{
	return m_World->GetCurrentTime() - m_ConnectionTime;
}

FakePlayer& FakePlayer::AddFriend(const SteamID& id)
{
	if (!m_Friends.has_value())
		m_Friends = SteamAPI::PlayerFriends{};

	m_Friends.value().m_Friends.insert(id);
	return *this;
}

const std::any* FakePlayer::FindDataStorage(const std::type_index& type) const
{
	if (auto found = m_Data.find(type); found != m_Data.end())
		return &found->second;

	return nullptr;
}

std::optional<time_point_t> FakeAccountAges::EstimateAccountCreationTime(const SteamID& id) const
{
	if (auto found = m_Ages.find(id); found != m_Ages.end())
		return found->second;

	return std::nullopt;
}

// ---------------------------------------------------------------------------------------------------------------------
// FakeWorldState

FakeWorldState::FakeWorldState()
{
	// fixed, so anything time-based is deterministic (2023-11-14 22:13:20 UTC)
	m_CurrentTime = time_point_t(std::chrono::duration_cast<time_point_t::duration>(std::chrono::seconds(1'700'000'000)));
	m_LastStatusUpdateTime = m_CurrentTime;
}

FakePlayer& FakeWorldState::AddPlayer(SteamID id, std::string name, TFTeam team)
{
	if (auto existing = FindFakePlayer(id))
	{
		existing->m_Name = std::move(name);
		existing->m_Team = team;
		return *existing;
	}

	auto& player = m_Players.emplace_back(std::make_shared<FakePlayer>(*this, id, std::move(name)));
	player->m_Team = team;
	return *player;
}

FakePlayer* FakeWorldState::FindFakePlayer(const SteamID& id)
{
	for (auto& player : m_Players)
	{
		if (player->m_SteamID == id)
			return player.get();
	}

	return nullptr;
}

void FakeWorldState::RemovePlayer(const SteamID& id)
{
	std::erase_if(m_Players, [&](const std::shared_ptr<FakePlayer>& p) { return p->m_SteamID == id; });
}

void FakeWorldState::ClearPlayers()
{
	m_Players.clear();
}

void FakeWorldState::ResetScoreboard()
{
	m_ScoreboardResets++;
	for (auto& player : m_Players)
		player->m_Scores = {};
}

void FakeWorldState::AddWorldEventListener(IWorldEventListener* listener)
{
	if (std::find(m_WorldEventListeners.begin(), m_WorldEventListeners.end(), listener) == m_WorldEventListeners.end())
		m_WorldEventListeners.push_back(listener);
}

void FakeWorldState::RemoveWorldEventListener(IWorldEventListener* listener)
{
	std::erase(m_WorldEventListeners, listener);
}

void FakeWorldState::AddConsoleLineListener(IConsoleLineListener* listener)
{
	if (std::find(m_ConsoleLineListeners.begin(), m_ConsoleLineListeners.end(), listener) == m_ConsoleLineListeners.end())
		m_ConsoleLineListeners.push_back(listener);
}

void FakeWorldState::RemoveConsoleLineListener(IConsoleLineListener* listener)
{
	std::erase(m_ConsoleLineListeners, listener);
}

void FakeWorldState::AddConsoleOutputChunk(const std::string_view& chunk)
{
	size_t start = 0;
	while (start < chunk.size())
	{
		size_t end = chunk.find('\n', start);
		if (end == chunk.npos)
			end = chunk.size();

		auto line = chunk.substr(start, end - start);
		if (!line.empty() && line.back() == '\r')
			line.remove_suffix(1);
		if (!line.empty())
			m_ConsoleOutput.emplace_back(line);

		start = end + 1;
	}
}

mh::task<> FakeWorldState::AddConsoleOutputLine(std::string line)
{
	m_ConsoleOutput.push_back(std::move(line));
	co_return;
}

bool FakeWorldState::ApplyG15Dump(const std::vector<G15PlayerData>&)
{
	m_G15DumpsApplied++;
	return true;
}

std::optional<SteamID> FakeWorldState::FindSteamIDForName(const std::string_view& playerName) const
{
	for (const auto& player : m_Players)
	{
		if (player->m_Name == playerName)
			return player->m_SteamID;
	}

	return std::nullopt;
}

std::optional<LobbyMemberTeam> FakeWorldState::FindLobbyMemberTeam(const SteamID& id) const
{
	if (auto player = FindPlayer(id))
	{
		if (auto member = player->GetLobbyMember())
			return member->m_Team;
	}

	return std::nullopt;
}

std::optional<UserID_t> FakeWorldState::FindUserID(const SteamID& id) const
{
	if (auto player = FindPlayer(id))
		return player->GetUserID();

	return std::nullopt;
}

TeamShareResult FakeWorldState::CompareTeams(const std::optional<LobbyMemberTeam>& team0,
	const std::optional<LobbyMemberTeam>& team1)
{
	// same rules as WorldState
	if (!team0 || !team1)
		return TeamShareResult::Neither;

	return (*team0 == *team1) ? TeamShareResult::SameTeams : TeamShareResult::OppositeTeams;
}

TeamShareResult FakeWorldState::GetTeamShareResult(const SteamID& id) const
{
	return GetTeamShareResult(id, m_LocalSteamID);
}

TeamShareResult FakeWorldState::GetTeamShareResult(const SteamID& id0, const SteamID& id1) const
{
	return CompareTeams(FindLobbyMemberTeam(id0), FindLobbyMemberTeam(id1));
}

TeamShareResult FakeWorldState::GetTeamShareResult(const std::optional<LobbyMemberTeam>& team0, const SteamID& id1) const
{
	return CompareTeams(team0, FindLobbyMemberTeam(id1));
}

const IPlayer* FakeWorldState::FindPlayer(const SteamID& id) const
{
	for (const auto& player : m_Players)
	{
		if (player->m_SteamID == id)
			return player.get();
	}

	return nullptr;
}

size_t FakeWorldState::GetApproxLobbyMemberCount() const
{
	return static_cast<size_t>(std::count_if(m_Players.begin(), m_Players.end(),
		[](const std::shared_ptr<FakePlayer>& p) { return p->m_LobbyMember.has_value(); }));
}

mh::generator<const IPlayer&> FakeWorldState::GetLobbyMembers() const
{
	for (const auto& player : m_Players)
	{
		if (player->m_LobbyMember)
			co_yield *player;
	}
}

mh::generator<const IPlayer&> FakeWorldState::GetPlayers() const
{
	for (const auto& player : m_Players)
		co_yield *player;
}

void FakeWorldState::Broadcaster::OnConsoleLineParsed(IWorldState& world, IConsoleLine& line)
{
	for (auto* listener : m_World.m_ConsoleLineListeners)
		listener->OnConsoleLineParsed(world, line);
}

void FakeWorldState::Broadcaster::OnConsoleLineUnparsed(IWorldState& world, const std::string_view& text)
{
	for (auto* listener : m_World.m_ConsoleLineListeners)
		listener->OnConsoleLineUnparsed(world, text);
}

void FakeWorldState::Broadcaster::OnConsoleLogChunkParsed(IWorldState& world, bool consoleLinesParsed)
{
	for (auto* listener : m_World.m_ConsoleLineListeners)
		listener->OnConsoleLogChunkParsed(world, consoleLinesParsed);
}

// ---------------------------------------------------------------------------------------------------------------------
// Sample data

void tf2_bot_detector::Testing::PopulateSampleLobby(FakeWorldState& world)
{
	world.ClearPlayers();
	world.m_ConsoleOutput.clear();

	static constexpr std::array<const char*, 12> NAMES =
	{
		"LocalPlayer", "BestFriend", "Pyro Main", "medic pls", "xX_Sniper_Xx", "Heavy Weapons Guy",
		"Engie", "Spy Crab", "DemoKnight", "Scout's Mom", "Private Profile", "connecting...",
	};

	for (uint32_t i = 0; i < NAMES.size(); i++)
	{
		const bool red = i < 6;
		auto& p = world.AddPlayer(FakeSteamID(i), NAMES[i], red ? TFTeam::Red : TFTeam::Blue);

		p.m_LobbyMember = LobbyMember
		{
			.m_SteamID = p.m_SteamID,
			.m_Index = i,
			.m_Team = red ? LobbyMemberTeam::Invaders : LobbyMemberTeam::Defenders,
			.m_Type = LobbyMemberType::Player,
			.m_Pending = false,
		};
		p.m_UserID = UserID_t(100 + i);
		p.m_Ping = uint16_t(30 + i * 7);
		p.m_Scores.m_Kills = uint16_t((i * 5) % 17);
		p.m_Scores.m_Deaths = uint16_t((i * 3) % 11);
		p.m_ConnectionTime = world.m_CurrentTime - std::chrono::minutes(5 + i * 2);
		p.m_ActiveTime = std::chrono::minutes(4 + i * 2);

		SteamAPI::PlayerSummary summary{};
		summary.m_SteamID = p.m_SteamID;
		summary.m_Nickname = p.m_Name;
		p.m_Summary = summary;

		SteamAPI::PlayerBans bans{};
		bans.m_SteamID = p.m_SteamID;
		p.m_Bans = bans;

		p.m_TF2Playtime = duration_t(std::chrono::hours(100 + i * 50));
		world.m_AccountAges.OnDataReady(p.m_SteamID, world.m_CurrentTime - std::chrono::hours(24 * 365 * (1 + i % 8)));
		p.m_EstimatedAccountCreationTime = world.m_AccountAges.EstimateAccountCreationTime(p.m_SteamID);
	}

	world.SetLocalPlayer(FakeSteamID(0));

	// a Steam friend of the local player, on the same team
	world.FindFakePlayer(FakeSteamID(1))->m_IsFriend = true;
	world.FindFakePlayer(FakeSteamID(0))->AddFriend(FakeSteamID(1));

	// public friends list with two people in this match
	world.FindFakePlayer(FakeSteamID(7))->AddFriend(FakeSteamID(8)).AddFriend(FakeSteamID(9));
	world.FindFakePlayer(FakeSteamID(8))->AddFriend(FakeSteamID(7));

	// private profile: friends lookups fail
	world.FindFakePlayer(FakeSteamID(10))->m_Friends = make_error_condition(std::errc::permission_denied);

	// still connecting, nothing fetched yet
	{
		auto& p = *world.FindFakePlayer(FakeSteamID(11));
		p.m_State = PlayerStatusState::Connecting;
		p.m_Team = TFTeam::Unknown;
		p.m_UserID.reset();
		p.m_Summary = NotLoadedError();
		p.m_Bans = NotLoadedError();
		p.m_TF2Playtime = NotLoadedError();
		p.m_EstimatedAccountCreationTime.reset();
	}

	world.m_VoteInProgress = true;
	world.AddConsoleOutputChunk(
		"Pyro Main :  gg\n"
		"*DEAD* xX_Sniper_Xx :  nice shot\n"
		"(TEAM) medic pls :  uber ready\n");
}
