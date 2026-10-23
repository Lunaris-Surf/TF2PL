#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace tf2_bot_detector::WebHostAlias
{
	inline constexpr char NAME[] = "local.tf2pl";
	inline constexpr char ENTRY[] = "127.0.0.1 local.tf2pl # TF2PL Web UI";
	enum class State { Missing, Ready, Conflict };

	inline std::string Lower(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return value;
	}

	// Inspect every active alias, including aliases after the canonical name.
	// A conflicting IPv6 entry also matters: the server listens on IPv4 only.
	inline State Inspect(std::string_view contents)
	{
		State state = State::Missing;
		std::istringstream lines{ std::string(contents) };
		std::string line;
		while (std::getline(lines, line))
		{
			line.erase(line.find('#') == line.npos ? line.size() : line.find('#'));
			std::istringstream fields(line);
			std::string address, name;
			if (!(fields >> address))
				continue;
			while (fields >> name)
			{
				if (!name.empty() && name.back() == '.')
					name.pop_back();
				if (Lower(name) != NAME)
					continue;
				if (address != "127.0.0.1")
					return State::Conflict;
				state = State::Ready;
			}
		}
		return state;
	}

	inline bool IsAllowedHost(std::string host, uint16_t port)
	{
		host = Lower(std::move(host));
		for (const char* name : { "127.0.0.1", "localhost", NAME })
			if (host == std::string(name) + ":" + std::to_string(port) || (port == 80 && host == name))
				return true;
		return false;
	}

	// Checks the fixed hosts entry, then launches only the small setup helper with
	// OS elevation if necessary. Never elevates the application/server itself.
	bool EnsureInstalled();
}
