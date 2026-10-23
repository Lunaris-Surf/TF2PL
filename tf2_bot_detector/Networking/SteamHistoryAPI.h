#pragma once

#include "SteamID.h"
#include "HTTPClient.h"

#include <tf2pl/coroutine/task.hpp>
#include <tf2pl/reflection/enum.hpp>
#include <nlohmann/json_fwd.hpp>

#include <memory>
#include <vector>
#include <unordered_map>
#include "Clock.h"

namespace tf2_bot_detector
{
	class IHTTPClient;
}

namespace tf2_bot_detector::SteamHistoryAPI
{
	enum BanState {
		Expired,
		Unbanned,
		Current, // "Temp-Ban"
		Permanent
	};

	enum class ErrorCode
	{
		OK,
		FeatureDisabled,
		InvalidAPIKey
	};

	struct PlayerSourceBan {
		SteamID m_ID;

		BanState m_BanState = BanState::Expired;

		/// <summary>
		/// this user was banned as this username.
		/// </summary>
		std::string m_UserName = "Unknown";

		std::string m_BanReason = "";
		std::string m_UnbanReason = "";

		time_point_t m_BanTimestamp;
		time_point_t m_UnbanTimestamp;

		std::string m_Server;
	};

	typedef std::vector<PlayerSourceBan> PlayerSourceBans;
	typedef std::unordered_map<SteamID, PlayerSourceBans> PlayerSourceBansResponse;

	typedef std::unordered_map<std::string, PlayerSourceBan> PlayerSourceBanState;

	void from_json(const nlohmann::json& j, BanState& d);
	void from_json(const nlohmann::json& j, PlayerSourceBan& d);
	void from_json(const nlohmann::json& j, PlayerSourceBansResponse& d);

	tf2pl::task<PlayerSourceBansResponse> GetPlayerSourceBansAsync(const std::string& apiKey, const std::vector<SteamID>& steamIDs, const HTTPClient& client);
}

// for our logging functions.
TF2PL_ENUM_REFLECT_BEGIN(tf2_bot_detector::SteamHistoryAPI::BanState)
	TF2PL_ENUM_REFLECT_VALUE(Expired)
	TF2PL_ENUM_REFLECT_VALUE(Unbanned)
	TF2PL_ENUM_REFLECT_VALUE(Current)
	TF2PL_ENUM_REFLECT_VALUE(Permanent)
TF2PL_ENUM_REFLECT_END()
