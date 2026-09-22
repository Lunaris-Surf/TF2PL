#include "Config/Rules.h"
#include "GameData/IPlayer.h"
#include "Networking/SteamAPI.h"
#include "Networking/SteamHistoryAPI.h"
#include <nlohmann/json.hpp>

#include <mh/error/not_implemented_error.hpp>
#include <mh/text/codecvt.hpp>

#include <catch2/catch.hpp>

using namespace std::string_view_literals;
using namespace tf2_bot_detector;

namespace
{
	struct MockPlayer : IPlayer
	{
		std::string m_Name;
		const mh::expected<SteamHistoryAPI::PlayerSourceBanState>& GetPlayerSourceBanState() const override { throw mh::not_implemented_error(); }
		const mh::expected<SteamHistoryAPI::PlayerSourceBans>& GetPlayerSourceBans() const override { throw mh::not_implemented_error(); }
		const mh::expected<SteamAPI::PlayerFriends>& GetFriendsInfo() const override { throw mh::not_implemented_error(); }

		const IWorldState& GetWorld() const override { throw mh::not_implemented_error(); }

		// Inherited via IPlayer
		const LobbyMember* GetLobbyMember() const override
		{
			throw mh::not_implemented_error();
		}
		std::string GetNameUnsafe() const override { return m_Name; }
		SteamID GetSteamID() const override
		{
			throw mh::not_implemented_error();
		}
		const mh::expected<SteamAPI::PlayerSummary, std::error_condition>& GetPlayerSummary() const override
		{
			throw mh::not_implemented_error();
		}
		const mh::expected<SteamAPI::PlayerBans, std::error_condition>& GetPlayerBans() const override
		{
			throw mh::not_implemented_error();
		}
		mh::expected<duration_t, std::error_condition> GetTF2Playtime() const override
		{
			throw mh::not_implemented_error();
		}
		bool IsFriend() const override
		{
			throw mh::not_implemented_error();
		}
		std::optional<UserID_t> GetUserID() const override
		{
			throw mh::not_implemented_error();
		}
		PlayerStatusState GetConnectionState() const override
		{
			throw mh::not_implemented_error();
		}
		time_point_t GetConnectionTime() const override
		{
			throw mh::not_implemented_error();
		}
		duration_t GetConnectedTime() const override
		{
			throw mh::not_implemented_error();
		}
		TFTeam GetTeam() const override
		{
			throw mh::not_implemented_error();
		}
		const PlayerScores& GetScores() const override
		{
			throw mh::not_implemented_error();
		}
		uint16_t GetPing() const override
		{
			throw mh::not_implemented_error();
		}
		time_point_t GetLastStatusUpdateTime() const override
		{
			throw mh::not_implemented_error();
		}
		duration_t GetActiveTime() const override
		{
			throw mh::not_implemented_error();
		}
		std::any& GetOrCreateDataStorage(const std::type_index& type) override
		{
			throw mh::not_implemented_error();
		}
		const std::any* FindDataStorage(const std::type_index& type) const override
		{
			throw mh::not_implemented_error();
		}
		std::optional<time_point_t> GetEstimatedAccountCreationTime() const override
		{
			throw mh::not_implemented_error();
		}
		const mh::expected<LogsTFAPI::PlayerLogsInfo>& GetLogsInfo() const override
		{
			throw mh::not_implemented_error();
		}
		const mh::expected<SteamAPI::PlayerInventoryInfo>& GetInventoryInfo() const override
		{
			throw mh::not_implemented_error();
		}
	};
}

TEST_CASE("Player Rules - ends_with", "[PlayerRuleTests]")
{
	MockPlayer player;
	player.m_Name = "Special Gamer";

	ModerationRule rule;
	rule.m_Description = "test rule - ends_with";

	auto& usernameTextMatch = rule.m_Triggers.m_UsernameTextMatch.emplace();
	usernameTextMatch.m_Mode = TextMatchMode::EndsWith;

	usernameTextMatch.m_Patterns = { "Special Gamer" };
	REQUIRE(rule.Match(player));

	usernameTextMatch.m_Patterns = { "Super Special Gamer" };
	REQUIRE(!rule.Match(player));

	usernameTextMatch.m_Patterns = { "Gamer" };
	REQUIRE(rule.Match(player));

	usernameTextMatch.m_Patterns = { "Gamers" };
	REQUIRE(!rule.Match(player));

	usernameTextMatch.m_Patterns = { "Gamer", "Gamers" };
	REQUIRE(rule.Match(player));

	usernameTextMatch.m_Patterns = { "r" };
	REQUIRE(rule.Match(player));
}

TEST_CASE("Player Rules - chatmsg contains", "[PlayerRuleTests]")
{
	MockPlayer player;
	player.m_Name = "Special Gamer";

	const auto chatMsg = mh::change_encoding<char>(u8"Mean words!!!!!!!!!!!!!!! 😡");

	ModerationRule rule;

	SECTION("match any")
	{
		rule.m_Triggers.m_Mode = TriggerMatchMode::MatchAny;
	}
	SECTION("match all")
	{
		rule.m_Triggers.m_Mode = TriggerMatchMode::MatchAll;
	}

	auto& textMatch = rule.m_Triggers.m_ChatMsgTextMatch.emplace();
	textMatch.m_Mode = TextMatchMode::Contains;

	textMatch.m_Patterns = { "text" };
	REQUIRE(!rule.Match(player, chatMsg));

	textMatch.m_Patterns = { "ean" };
	REQUIRE(rule.Match(player, chatMsg));
}

TEST_CASE("Player Rules - chatmsg word", "[PlayerRuleTests]")
{
	MockPlayer player;
	player.m_Name = "Special Gamer";

	const auto chatMsg = mh::change_encoding<char>(u8"you are stinky");

	ModerationRule rule;

	SECTION("match any")
	{
		rule.m_Triggers.m_Mode = TriggerMatchMode::MatchAny;
	}
	SECTION("match all")
	{
		rule.m_Triggers.m_Mode = TriggerMatchMode::MatchAll;
	}

	auto& textMatch = rule.m_Triggers.m_ChatMsgTextMatch.emplace();
	textMatch.m_Mode = TextMatchMode::Word;

	textMatch.m_Patterns = { "you" };
	REQUIRE(rule.Match(player, chatMsg));

	textMatch.m_Patterns = { "are" };
	REQUIRE(rule.Match(player, chatMsg));

	textMatch.m_Patterns = { "stinky" };
	REQUIRE(rule.Match(player, chatMsg));

	textMatch.m_Patterns = { "smelly" };
	REQUIRE(!rule.Match(player, chatMsg));
}

TEST_CASE("LunarisV shared chat and SourceBan rules", "[PlayerRuleTests]")
{
	MockPlayer player;
	player.m_Name = "Example";
	ModerationRule rule;
	rule.m_Triggers.m_ChatMsgTextMatch = TextMatch{TextMatchMode::Contains, {"hostile"}, false};
	REQUIRE(rule.Match(player, "hostile message"));
	REQUIRE_FALSE(rule.Match(player, {}, "hostile ban reason"));
	rule.m_MatchSourceBans = true;
	REQUIRE(rule.Match(player, {}, "hostile ban reason"));
	REQUIRE_FALSE(rule.Match(player, {}, "ordinary reason"));
	rule.m_MatchChat = false;
	REQUIRE_FALSE(rule.Match(player, "hostile message"));
	REQUIRE(rule.Match(player, {}, "hostile ban reason"));
	REQUIRE_FALSE(rule.Match(player));
	rule.m_Triggers.m_UsernameTextMatch = TextMatch{TextMatchMode::Equal, {"Someone else"}, false};
	REQUIRE_FALSE(rule.Match(player, {}, "hostile ban reason"));
	rule.m_Triggers.m_Mode = TriggerMatchMode::MatchAny;
	REQUIRE(rule.Match(player, {}, "hostile ban reason"));
	player.m_Name = "Someone else";
	REQUIRE_FALSE(rule.Match(player, {}, "ordinary reason"));
}

TEST_CASE("LunarisV custom tag roundtrip and set operations", "[PlayerRuleTests]")
{
	const nlohmann::json input = {"racist", "vac_banned", "game_banned", "sourcebanned", "pedophilia", "custom:watchlist"};
	const auto tags = input.get<PlayerAttributesList>();
	REQUIRE(tags.count() == 6);
	const auto restored = nlohmann::json(tags).get<PlayerAttributesList>();
	REQUIRE(restored == tags);
	PlayerAttributesList custom;
	custom.SetCustomTag("custom:watchlist");
	REQUIRE((tags & custom) == custom);
	REQUIRE((tags | custom) == tags);
	REQUIRE(custom.SetCustomTag("custom:watchlist", false));
	REQUIRE(custom.empty());
	REQUIRE_FALSE(PlayerAttributesList::IsValidCustomTag("custom:friends"));
	REQUIRE_FALSE(PlayerAttributesList::IsValidCustomTag("custom:sore_losers"));
	REQUIRE_FALSE(PlayerAttributesList::IsValidCustomTag("custom:"));
	REQUIRE_THROWS(nlohmann::json({"not_a_builtin"}).get<PlayerAttributesList>());
	const auto removal = nlohmann::json({"cheater", "suspicious", "suspected_cheater"}).get<PlayerAttributesList>();
	REQUIRE(removal.count() == 3);
	REQUIRE(nlohmann::json(removal).get<PlayerAttributesList>() == removal);
}

TEST_CASE("LunarisV rule sources and custom actions survive JSON", "[PlayerRuleTests]")
{
	const auto input = nlohmann::json::parse(R"({"description":"test", "sources":["chat","sourcebans"],
		"triggers":{"chatmsg_text_match":{"mode":"word","patterns":["hostile"]}},
		"actions":{"mark":["custom:watchlist"],"transient_mark":["blacklisted"],"unmark":["suspicious"]}})");
	const auto rule = input.get<ModerationRule>();
	const auto restored = nlohmann::json(rule).get<ModerationRule>();
	REQUIRE(restored.m_MatchChat);
	REQUIRE(restored.m_MatchSourceBans);
	REQUIRE(restored.m_Actions.m_Mark.GetCustomTags().contains("custom:watchlist"));
	REQUIRE(restored.m_Actions.m_TransientMark.HasAttribute(PlayerAttribute::Blacklisted));
	REQUIRE(restored.m_Actions.m_Unmark.HasAttribute(PlayerAttribute::Suspicious));
}

TEST_CASE("LunarisV SteamHistory nullable fields and timestamps", "[PlayerRuleTests]")
{
	auto input = nlohmann::json::parse(R"({"SteamID":"[U:1:1234]", "CurrentState":"Permanent",
		"Name":null,"BanReason":"hostile", "BanTimestamp":"1700000000", "UnbanTimestamp":null,"Server":"example"})");
	const auto ban = input.get<SteamHistoryAPI::PlayerSourceBan>();
	REQUIRE(ban.m_BanReason == "hostile");
	REQUIRE(ban.m_BanTimestamp == time_point_t(std::chrono::seconds(1700000000)));
	input["BanTimestamp"] = 1700000000;
	REQUIRE(input.get<SteamHistoryAPI::PlayerSourceBan>().m_BanTimestamp == ban.m_BanTimestamp);
	input["CurrentState"] = "unexpected";
	REQUIRE_THROWS(input.get<SteamHistoryAPI::PlayerSourceBan>());
}
