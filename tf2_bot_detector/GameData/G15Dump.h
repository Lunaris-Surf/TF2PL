#pragma once

#include "GameData/TFConstants.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tf2_bot_detector
{
	/// <summary>
	/// One player's row from TF2's `g15_dumpplayer` command (needs -g15 on the TF2 command line).
	/// The dump is a set of per-slot arrays (m_szName[3], m_iPing[3], ...), indexed by entity index.
	/// </summary>
	struct G15PlayerData
	{
		uint8_t m_Index = 0;

		std::optional<std::string> m_Name;
		std::optional<uint32_t> m_AccountID; // steamID3 [U:1:<m_AccountID>]
		std::optional<UserID_t> m_UserID;
		std::optional<uint16_t> m_Ping;
		std::optional<TFTeam> m_Team;
		std::optional<int32_t> m_Health;
		std::optional<int32_t> m_Score;
		std::optional<int32_t> m_Deaths;

		bool m_Valid = false;
		bool m_Connected = false;
		bool m_Alive = false;
	};

	/// <summary>
	/// Max player slots TF2 can report (100 players since the July 2023 update), plus slot 0 and slack.
	/// </summary>
	inline constexpr size_t G15_MAX_SLOTS = 102;

	/// <summary>
	/// True if the reply means the command doesn't exist (TF2 wasn't launched with -g15).
	/// </summary>
	bool IsG15CommandUnavailable(std::string_view reply);

	/// <summary>
	/// Parses a `g15_dumpplayer` reply. Only returns slots that are valid and have a non-zero userid.
	/// </summary>
	std::vector<G15PlayerData> ParseG15Dump(std::string_view reply);
}
