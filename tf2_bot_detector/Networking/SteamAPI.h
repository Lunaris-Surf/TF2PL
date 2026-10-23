#pragma once

#include "Bitmap.h"
#include "Clock.h"
#include "SteamID.h"

#include <tf2pl/coroutine/task.hpp>
#include <tf2pl/error/error_code_exception.hpp>
#include <nlohmann/json_fwd.hpp>

#include <optional>
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

namespace tf2_bot_detector
{
	class IHTTPClient;
	class ISteamAPISettings;

	enum class PlayerAttribute;
}

namespace tf2_bot_detector::SteamAPI
{
	enum class ErrorCode
	{
		Success,

		EmptyState,
		InfoPrivate,
		GameNotOwned,
		UnexpectedDataFormat,
		GenericHttpError,
		JSONParseError,
		JSONDeserializeError,
		InvalidSteamID,
		EmptyAPIKey,
		SteamAPIDisabled,
	};

	struct SteamAPIError : tf2pl::error_condition_exception, std::nested_exception
	{
		SteamAPIError(std::error_condition code, const std::string_view& detail = {}, TF2PL_SOURCE_LOCATION_AUTO(location));

		tf2pl::source_location m_SourceLocation;
	};

	std::error_condition make_error_condition(tf2_bot_detector::SteamAPI::ErrorCode e);
}

namespace std
{
	template<> struct is_error_condition_enum<tf2_bot_detector::SteamAPI::ErrorCode> : std::bool_constant<true> {};
}

namespace tf2_bot_detector::SteamAPI
{
	enum class CommunityVisibilityState
	{
		Private = 1,
		FriendsOnly = 2,
		Public = 3,
	};

	enum class PersonaState
	{
		Offline = 0,
		Online = 1,
		Busy = 2,
		Away = 3,
		Snooze = 4,
		LookingToTrade = 5,
		LookingToPlay = 6,
	};

	enum class AvatarQuality
	{
		Small,
		Medium,
		Large,
	};

	struct PlayerSummary
	{
		SteamID m_SteamID;
		std::string m_RealName;
		std::string m_Nickname;
		std::string m_AvatarHash;
		std::string m_ProfileURL;
		PersonaState m_Status{};
		CommunityVisibilityState m_Visibility{};
		bool m_ProfileConfigured = false;
		bool m_CommentPermissions = false;
		std::optional<time_point_t> m_CreationTime;
		std::optional<time_point_t> m_LastLogOff;

		std::optional<duration_t> GetAccountAge() const;

		std::string GetAvatarURL(AvatarQuality quality = AvatarQuality::Large) const;
		tf2pl::task<Bitmap> GetAvatarBitmap(std::shared_ptr<const IHTTPClient> client,
			AvatarQuality quality = AvatarQuality::Large) const;

		std::string_view GetVanityURL() const;
	};
	void from_json(const nlohmann::json& j, PlayerSummary& d);

	tf2pl::task<std::vector<PlayerSummary>> GetPlayerSummariesAsync(const ISteamAPISettings& apiSettings,
		const std::vector<SteamID>& steamIDs, const IHTTPClient& client);

	enum class PlayerEconomyBan
	{
		None,
		Probation,
		Banned,

		Unknown,
	};

	struct PlayerBans
	{
		bool HasAnyBans() const;

		SteamID m_SteamID;
		bool m_CommunityBanned = false;
		PlayerEconomyBan m_EconomyBan{};
		unsigned m_VACBanCount = 0;
		unsigned m_GameBanCount = 0;
		duration_t m_TimeSinceLastBan{};
	};
	void from_json(const nlohmann::json& j, PlayerBans& d);

	tf2pl::task<std::vector<PlayerBans>> GetPlayerBansAsync(const ISteamAPISettings& apiSettings,
		const std::vector<SteamID>& steamIDs, const IHTTPClient& client);

	tf2pl::task<duration_t> GetTF2PlaytimeAsync(const ISteamAPISettings& apiSettings,
		const SteamID& steamID, const IHTTPClient& client);


	struct PlayerFriends {
		std::unordered_set<SteamID> m_Friends;

		// mark source : mark count?
		// std::unordered_map<std::string, uint32_t> m_MarkedCheaterCount;
		// bool m_Processed = false;
		// std::unordered_map<PlayerAttribute, uint32_t> m_MarkedCheaterCount;
		// uint32_t m_MarkedFriendsTotal = 0;
	};

	tf2pl::task<std::unordered_set<SteamID>> GetFriendList(const ISteamAPISettings& apiSettings,
		const SteamID& steamID, const IHTTPClient& client);

	struct PlayerInventoryInfo
	{
		uint32_t m_Items = 0;
		uint32_t m_Slots = 0;
	};
	tf2pl::task<PlayerInventoryInfo> GetTF2InventoryInfoAsync(const ISteamAPISettings& apiSettings, const SteamID& steamID, const IHTTPClient& client);

	std::string GenerateSteamIDsQueryParam(const std::vector<SteamID>& steamIDs, size_t max, TF2PL_SOURCE_LOCATION_AUTO(location));
}
