#include "G15Dump.h"

#include <array>
#include <charconv>

using namespace tf2_bot_detector;

namespace
{
	std::string_view Trim(std::string_view s)
	{
		while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r'))
			s.remove_prefix(1);
		while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
			s.remove_suffix(1);
		return s;
	}

	template<typename T>
	std::optional<T> ParseInt(std::string_view s)
	{
		T value{};
		const auto result = std::from_chars(s.data(), s.data() + s.size(), value);
		if (result.ec != std::errc{} || result.ptr != s.data() + s.size())
			return std::nullopt;
		return value;
	}

	std::optional<bool> ParseBool(std::string_view s)
	{
		if (s == "true")
			return true;
		if (s == "false")
			return false;
		return std::nullopt;
	}

	std::optional<TFTeam> ParseTeam(std::string_view s)
	{
		// Source engine team numbers: 0 unassigned, 1 spectator, 2 RED, 3 BLU
		switch (ParseInt<int>(s).value_or(-1))
		{
		case 0: return TFTeam::Unknown;
		case 1: return TFTeam::Spectator;
		case 2: return TFTeam::Red;
		case 3: return TFTeam::Blue;
		default: return std::nullopt;
		}
	}
}

bool tf2_bot_detector::IsG15CommandUnavailable(std::string_view reply)
{
	return reply.find("Unknown command") != reply.npos && reply.find("g15_dumpplayer") != reply.npos;
}

std::vector<G15PlayerData> tf2_bot_detector::ParseG15Dump(std::string_view reply)
{
	std::array<G15PlayerData, G15_MAX_SLOTS> slots{};

	size_t lineStart = 0;
	while (lineStart < reply.size())
	{
		size_t lineEnd = reply.find('\n', lineStart);
		if (lineEnd == reply.npos)
			lineEnd = reply.size();

		const std::string_view line = Trim(reply.substr(lineStart, lineEnd - lineStart));
		lineStart = lineEnd + 1;

		// Lines look like: m_iPing[2] integer (15)
		// Anything else (the "(localplayer)" block, m_Shared.*, etc) is skipped.
		if (!line.starts_with("m_"))
			continue;

		const size_t openBracket = line.find('[');
		if (openBracket == line.npos)
			continue;
		const size_t closeBracket = line.find(']', openBracket);
		if (closeBracket == line.npos)
			continue;

		const auto index = ParseInt<size_t>(line.substr(openBracket + 1, closeBracket - openBracket - 1));
		if (!index || *index >= slots.size())
			continue;

		// Value is between the first '(' after the type and the LAST ')' (names can contain parentheses).
		const size_t openParen = line.find('(', closeBracket);
		const size_t closeParen = line.rfind(')');
		if (openParen == line.npos || closeParen == line.npos || closeParen <= openParen)
			continue;

		const std::string_view field = line.substr(0, openBracket);
		const std::string_view value = line.substr(openParen + 1, closeParen - openParen - 1);
		G15PlayerData& slot = slots[*index];
		slot.m_Index = static_cast<uint8_t>(*index);

		if (field == "m_szName")
			slot.m_Name = std::string(value);
		else if (field == "m_iAccountID")
			slot.m_AccountID = ParseInt<uint32_t>(value);
		else if (field == "m_iUserID")
			slot.m_UserID = ParseInt<UserID_t>(value);
		else if (field == "m_iPing")
			slot.m_Ping = ParseInt<uint16_t>(value);
		else if (field == "m_iTeam")
			slot.m_Team = ParseTeam(value);
		else if (field == "m_iHealth")
			slot.m_Health = ParseInt<int32_t>(value);
		else if (field == "m_iScore")
			slot.m_Score = ParseInt<int32_t>(value);
		else if (field == "m_iDeaths")
			slot.m_Deaths = ParseInt<int32_t>(value);
		else if (field == "m_bValid")
			slot.m_Valid = ParseBool(value).value_or(false);
		else if (field == "m_bConnected")
			slot.m_Connected = ParseBool(value).value_or(false);
		else if (field == "m_bAlive")
			slot.m_Alive = ParseBool(value).value_or(false);
	}

	std::vector<G15PlayerData> players;
	for (auto& slot : slots)
	{
		if (slot.m_Valid && slot.m_UserID.value_or(0) != 0)
			players.push_back(std::move(slot));
	}

	return players;
}
