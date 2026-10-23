#include "Tests/FakeWorldState.h"

#include <catch2/catch_test_macros.hpp>

using namespace std::chrono_literals;
using namespace tf2_bot_detector;
using namespace tf2_bot_detector::Testing;

TEST_CASE("FakeWorldState sample lobby shape", "[FakeWorldState]")
{
	FakeWorldState world;
	PopulateSampleLobby(world);

	size_t playerCount = 0;
	for (const IPlayer& player : world.GetPlayers())
	{
		(void)player;
		playerCount++;
	}
	REQUIRE(playerCount == 12);
	REQUIRE(world.GetApproxLobbyMemberCount() == 12);

	REQUIRE(world.IsLocalPlayerInitialized());
	REQUIRE(world.LocalPlayer() != nullptr);
	REQUIRE(world.LocalPlayer()->GetSteamID() == FakeSteamID(0));
	REQUIRE(world.IsVoteInProgress());
	REQUIRE(world.GetMapName() == "pl_upward");
}

TEST_CASE("FakeWorldState team sharing", "[FakeWorldState]")
{
	FakeWorldState world;
	PopulateSampleLobby(world);

	// players 0-5 are RED (invaders), 6-11 are BLU (defenders); the local player is 0
	REQUIRE(world.GetTeamShareResult(FakeSteamID(1)) == TeamShareResult::SameTeams);
	REQUIRE(world.GetTeamShareResult(FakeSteamID(6)) == TeamShareResult::OppositeTeams);
	REQUIRE(world.GetTeamShareResult(FakeSteamID(6), FakeSteamID(7)) == TeamShareResult::SameTeams);

	// someone who isn't in the lobby shares nothing
	REQUIRE(world.GetTeamShareResult(FakeSteamID(500)) == TeamShareResult::Neither);

	world.FindFakePlayer(FakeSteamID(1))->m_LobbyMember.reset();
	REQUIRE(world.GetTeamShareResult(FakeSteamID(1)) == TeamShareResult::Neither);
}

TEST_CASE("FakeWorldState lookups", "[FakeWorldState]")
{
	FakeWorldState world;
	PopulateSampleLobby(world);

	REQUIRE(world.FindSteamIDForName("Spy Crab") == FakeSteamID(7));
	REQUIRE_FALSE(world.FindSteamIDForName("nobody").has_value());

	REQUIRE(world.FindUserID(FakeSteamID(3)) == UserID_t(103));
	REQUIRE_FALSE(world.FindUserID(FakeSteamID(11)).has_value()); // still connecting

	const IPlayer* friendPlayer = world.FindPlayer(FakeSteamID(1));
	REQUIRE(friendPlayer);
	REQUIRE(friendPlayer->IsFriend());
	REQUIRE(&friendPlayer->GetWorld() == &world);
}

TEST_CASE("FakeWorldState friends data", "[FakeWorldState]")
{
	FakeWorldState world;
	PopulateSampleLobby(world);

	const auto& friends = world.FindPlayer(FakeSteamID(7))->GetFriendsInfo();
	REQUIRE(friends.has_value());
	REQUIRE(friends.value().m_Friends.size() == 2);
	REQUIRE(friends.value().m_Friends.contains(FakeSteamID(8)));

	// private profile and not-yet-loaded both report an error rather than an empty list
	REQUIRE_FALSE(world.FindPlayer(FakeSteamID(10))->GetFriendsInfo().has_value());
	REQUIRE_FALSE(world.FindPlayer(FakeSteamID(4))->GetFriendsInfo().has_value());
}

TEST_CASE("FakeWorldState console output and time", "[FakeWorldState]")
{
	FakeWorldState world;
	PopulateSampleLobby(world);

	REQUIRE(world.m_ConsoleOutput.size() == 3);
	REQUIRE(world.m_ConsoleOutput[0] == "Pyro Main :  gg");

	world.AddConsoleOutputChunk("one\r\ntwo\n\nthree");
	REQUIRE(world.m_ConsoleOutput.size() == 6);
	REQUIRE(world.m_ConsoleOutput.back() == "three");

	const auto before = world.GetCurrentTime();
	const auto connectedBefore = world.FindPlayer(FakeSteamID(2))->GetConnectedTime();
	world.AdvanceTime(30s);
	REQUIRE(world.GetCurrentTime() - before == duration_t(30s));
	REQUIRE(world.FindPlayer(FakeSteamID(2))->GetConnectedTime() - connectedBefore == duration_t(30s));
}

TEST_CASE("FakeWorldState scoreboard reset and removal", "[FakeWorldState]")
{
	FakeWorldState world;
	PopulateSampleLobby(world);

	world.ResetScoreboard();
	for (const IPlayer& player : world.GetPlayers())
		REQUIRE(player.GetScores().m_Kills == 0);

	world.RemovePlayer(FakeSteamID(5));
	REQUIRE(world.FindPlayer(FakeSteamID(5)) == nullptr);
	REQUIRE(world.GetApproxLobbyMemberCount() == 11);
}
