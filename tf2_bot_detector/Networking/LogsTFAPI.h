#pragma once

#include "SteamID.h"

#include <tf2pl/coroutine/task.hpp>

#include <memory>

namespace tf2_bot_detector
{
	class IHTTPClient;
}

namespace tf2_bot_detector::LogsTFAPI
{
	struct PlayerLogsInfo
	{
		SteamID m_ID;
		uint32_t m_LogsCount;
	};

	tf2pl::task<PlayerLogsInfo> GetPlayerLogsInfoAsync(std::shared_ptr<const IHTTPClient> client, SteamID id);
}
