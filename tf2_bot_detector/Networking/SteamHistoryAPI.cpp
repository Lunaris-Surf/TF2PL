#include "SteamAPI.h"
#include "SteamHistoryAPI.h"

#include "HTTPHelpers.h"

#include <fmt/format.h>
#include <fmt/std.h>
#include <fmt/ostream.h>
#include <fmt/chrono.h>
#include <fmt/xchar.h>
#include <nlohmann/json.hpp>
#include <charconv>
#include <stdexcept>

/// <summary>
/// gets sourcebans from XVF's steamhistory site.
///
/// largely copypasted from SteamAPI::GetPlayerSummariesAsync
/// </summary>
/// <param name="apiKey"></param>
/// <param name="steamIDs"></param>
/// <param name="client"></param>
/// <returns></returns>
mh::task<tf2_bot_detector::SteamHistoryAPI::PlayerSourceBansResponse>
	tf2_bot_detector::SteamHistoryAPI::GetPlayerSourceBansAsync(
		const std::string& apiKey,
		const std::vector<SteamID>& steamIDs,
		const HTTPClient& client
	)
{
	if (steamIDs.empty())
		co_return{};

	std::string requestSteamIDs = tf2_bot_detector::SteamAPI::GenerateSteamIDsQueryParam(steamIDs, 100);
	requestSteamIDs.at(0) = '&';

	// copied segments of GenerateSteamAPIURL; consolidate later?

	// Might have an option in the future that you can choose between sh api and roto's api
	// - which exists (https://bd-api.roto.lol/profile?steamids=<ids, comma seperated> apparently).
	// in case one or the other goes down.
	URL requestURL = URL(fmt::format(FMT_STRING("https://steamhistory.net/api/sourcebans?shouldkey=1&key={}{}"), apiKey, requestSteamIDs));

	auto clientPtr = client.shared_from_this();
	const std::string data = co_await clientPtr->GetStringAsync(requestURL);

	// Do not turn malformed/error responses into an empty successful ban history.
	const auto json = nlohmann::json::parse(data);
	const auto& response = json.at("response");
	if (!response.is_object()) throw std::runtime_error("Invalid SteamHistory response");
	co_return response.get<PlayerSourceBansResponse>();
}


void tf2_bot_detector::SteamHistoryAPI::from_json(const nlohmann::json& j, BanState& d) {
	if (j == "Permanent") {
		d = BanState::Permanent;
	}
	else if (j == "Temp-Ban") {
		d = BanState::Current;
	}
	else if (j == "Unbanned") {
		d = BanState::Unbanned;
	}
	else if (j == "Expired") {
		d = BanState::Expired;
	}
	else {
		throw std::invalid_argument("Unknown SteamHistory ban state");
	}
}

namespace
{
	std::string OptionalText(const nlohmann::json& json, const char* key)
	{
		const auto found = json.find(key);
		return found != json.end() && found->is_string() ? found->get<std::string>() : std::string{};
	}

	tf2_bot_detector::time_point_t ReadTimestamp(const nlohmann::json& json, const char* key)
	{
		const auto found = json.find(key);
		if (found == json.end() || found->is_null() || *found == "") return {};
		std::int64_t seconds = 0;
		if (found->is_string())
		{
			const auto value = found->get<std::string>();
			const auto parsed = std::from_chars(value.data(), value.data() + value.size(), seconds);
			if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
				throw std::invalid_argument("Invalid SteamHistory timestamp");
		}
		else if (found->is_number_integer()) seconds = found->get<std::int64_t>();
		else throw std::invalid_argument("Invalid SteamHistory timestamp type");
		// Bound the conversion before converting seconds to the system clock's finer duration.
		if (seconds < 0 || seconds > 7258118400LL) throw std::invalid_argument("SteamHistory timestamp out of range");
		return tf2_bot_detector::time_point_t(std::chrono::seconds(seconds));
	}
}

void tf2_bot_detector::SteamHistoryAPI::from_json(const nlohmann::json& j, PlayerSourceBan& d)
{
	d = {};
	d.m_ID = j.at("SteamID");
	d.m_UserName = OptionalText(j, "Name");
	d.m_BanState = j.at("CurrentState").get<BanState>();
	d.m_BanReason = OptionalText(j, "BanReason");
	d.m_UnbanReason = OptionalText(j, "UnbanReason");
	d.m_BanTimestamp = ReadTimestamp(j, "BanTimestamp");
	d.m_UnbanTimestamp = ReadTimestamp(j, "UnbanTimestamp");
	d.m_Server = OptionalText(j, "Server");
}

void tf2_bot_detector::SteamHistoryAPI::from_json(const nlohmann::json& j, PlayerSourceBansResponse& d)
{
	if (!j.is_object()) throw std::invalid_argument("Invalid SteamHistory response");
	d.clear();
	for (const auto& [key, value] : j.items())
	{
		const SteamID id(key);
		const auto bans = value.get<PlayerSourceBans>();
		for (const auto& ban : bans)
			if (ban.m_ID != id) throw std::invalid_argument("SteamHistory record SteamID mismatch");
		d.emplace(id, bans);
	}
}
