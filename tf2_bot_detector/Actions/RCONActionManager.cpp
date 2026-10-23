#include "RCONActionManager.h"
#include "Actions/ActionGenerators.h"
#include "Config/Settings.h"
#include "Actions.h"
#include "Log.h"
#include "WorldEventListener.h"
#include "WorldState.h"
#include "GameData/G15Dump.h"

#include <tf2pl/text/insertion_conversion.hpp>
#include <tf2pl/text/string_insertion.hpp>
#include <srcon/async_client.h>

#include <filesystem>
#include <iomanip>
#include <queue>
#include <regex>
#include <unordered_set>

#undef min
#undef max

static const std::regex s_SingleCommandRegex(R"regex(((?:\w+)(?:\ +\w+)?)(?:\n)?)regex", std::regex::optimize);

using namespace tf2_bot_detector;
using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;

struct SRCONInit
{
	SRCONInit()
	{
		srcon::SetLogFunc([](std::string&& msg)
			{
				DebugLog("[SRCON] "s << std::move(msg));
			});
	}

} s_SRCONInit;

std::unique_ptr<RCONActionManager> RCONActionManager::Create(const Settings& settings, IWorldState& world)
{
	return std::make_unique<RCONActionManager>(settings, world);
}

RCONActionManager::RCONActionManager(const Settings& settings, IWorldState& world) :
	m_Settings(settings), m_WorldState(world)
{
	world.AddWorldEventListener(this);

	m_IsDiscardingServerCommands = settings.m_ConfigCompatibilityMode;
}

RCONActionManager::~RCONActionManager()
{
}

bool RCONActionManager::QueueAction(std::unique_ptr<IAction>&& action)
{
	if (const auto maxQueuedCount = action->GetMaxQueuedCount();
		maxQueuedCount <= m_Actions.size())
	{
		const ActionType curActionType = action->GetType();
		size_t count = 0;
		for (const auto& queued : m_Actions)
		{
			if (queued->GetType() == curActionType)
			{
				if (++count >= maxQueuedCount)
					return false;
			}
		}
	}

	m_Actions.push_back(std::move(action));
	return true;
}

void RCONActionManager::AddPeriodicActionGenerator(std::unique_ptr<IPeriodicActionGenerator>&& action)
{
	m_PeriodicActionGenerators.push_back(std::move(action));
}

void RCONActionManager::OnLocalPlayerInitialized(IWorldState& world, bool initialized)
{
	m_IsDiscardingServerCommands = !initialized;
	DebugLog("m_IsDiscardingServerCommands = {}, m_Settings.m_ConfigCompatibilityMode = {}",
		m_IsDiscardingServerCommands, m_Settings.m_ConfigCompatibilityMode);
}

void RCONActionManager::ProcessRunningCommands()
{
	constexpr const char* funcName = __func__;
	const auto PrintErrorMsg = [funcName](const std::string_view& msg)
	{
		return DebugLogWarning(""s << funcName << "(): " << msg);
	};

	while (!m_RunningCommands.empty())
	{
		auto& cmd = m_RunningCommands.front();
		if (cmd.m_Future.wait_for(0s) == std::future_status::timeout)
			break;

		try
		{
			auto resultStr = cmd.m_Future.get();

			if (m_Settings.m_Unsaved.m_DebugShowCommands)
			{
				const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(tf2_bot_detector::clock_t::now() - cmd.m_StartTime);
				std::string msg = "Game command processed in "s << elapsed.count() << "ms : " << std::quoted(cmd.m_Command);

				if (!resultStr.empty())
					msg << ", response " << resultStr.size() << " bytes";

				DebugLog({ 1, 1, 1, 0.5f }, std::move(msg));
			}

			if (cmd.m_Command == "g15_dumpplayer")
			{
				// Handled directly: ~20KB of netprop dump we don't want going through the line parser.
				HandleG15Reply(resultStr, tfbd_clock_t::now());
			}
			else if (!resultStr.empty())
			{
				m_WorldState.AddConsoleOutputChunk(resultStr);
			}
		}
		catch (const std::future_error& e)
		{
			if (e.code() == std::future_errc::broken_promise) {
				DebugLogWarning(std::string(__FUNCTION__) << "(): " << e.code().message() << ": " << e.what() << ": " << std::quoted(cmd.m_Command));
			}
			else
				PrintErrorMsg(e.code().message() << ": " << e.what() << ": " << std::quoted(cmd.m_Command));
		}
		catch (const std::exception& e)
		{
			PrintErrorMsg(""s << e.what() << ": " << std::quoted(cmd.m_Command));
		}

		m_RunningCommands.pop();
	}
}

/// <summary>
/// should we send this command to rcon?
/// </summary>
/// <param name="cmd">the command</param>
/// <returns>true if we should discard this command</returns>
bool RCONActionManager::ShouldDiscardCommand(const std::string_view& cmd) const
{
	if (!m_IsDiscardingServerCommands || !m_Settings.m_ConfigCompatibilityMode)
		return false;

	static const std::unordered_set<std::string_view> s_KnownClientCommands =
	{
		"tf_lobby_debug",
		"tf_party_debug",
		"net_status",
		"con_logfile",
		"con_timestamp",
		"tf_mm_debug_level",
		"net_showmsg",
	};

	if (s_KnownClientCommands.contains(cmd))
		return false;

	DebugLog("Discarding potential server command "s << std::quoted(cmd));
	return true;
}

void RCONActionManager::ProcessQueuedCommands()
{
	if (!m_Settings.m_Unsaved.m_RCONClient)
		return;

	const auto curTime = tfbd_clock_t::now();
	if (curTime < (m_LastUpdateTime + UPDATE_INTERVAL))
		return;

	// Player data (g15_dumpplayer / status / ping)
	UpdatePlayerDataPolling(curTime);

	// Update periodic actions
	for (const auto& generator : m_PeriodicActionGenerators)
		generator->Execute(*this);

	if (!m_Actions.empty())
	{
		bool actionTypes[(int)ActionType::COUNT]{};

		struct Writer final : ICommandWriter
		{
			void Write(std::string cmd, std::string args) override
			{
				if (m_Manager->ShouldDiscardCommand(cmd))
					return;

				if (!args.empty())
					cmd << ' ' << args;

				m_Manager->m_RunningCommands.push(
					{
						.m_StartTime = tfbd_clock_t::now(),
						.m_Command = cmd,
						.m_Future = m_Manager->m_Settings.m_Unsaved.m_RCONClient->send_command_async(cmd, false),
					});
			}

			RCONActionManager* m_Manager = nullptr;

		} writer;

		writer.m_Manager = this;

		const auto ProcessAction = [&](const IAction* action)
		{
			const ActionType type = action->GetType();
			{
				auto& previousMsg = actionTypes[(int)type];
				const auto minInterval = action->GetMinInterval();

				if (minInterval.count() > 0 && (previousMsg || (curTime - m_LastTriggerTime[type]) < minInterval))
					return false;

				previousMsg = true;
			}

			action->WriteCommands(writer);
			m_LastTriggerTime[type] = curTime;
			return true;
		};

		// Process actions
		for (auto it = m_Actions.begin(); it != m_Actions.end(); )
		{
			const IAction* action = it->get();
			if (ProcessAction(it->get()))
				it = m_Actions.erase(it);
			else
				++it;
		}
	}

	m_LastUpdateTime = curTime;
}

bool RCONActionManager::IsUsingG15(time_point_t curTime)
{
	if (!m_Settings.m_UseG15PlayerData)
		return false;

	// TF2 may have been restarted with -g15 since we gave up; try again now and then.
	if (m_G15Unavailable && (curTime - m_G15UnavailableSince) >= G15_RETRY_INTERVAL)
	{
		m_G15Unavailable = false;
		m_G15EmptyReplies = 0;
	}

	return !m_G15Unavailable;
}

void RCONActionManager::UpdatePlayerDataPolling(time_point_t curTime)
{
	if (IsUsingG15(curTime))
	{
		// Primary source: names, userids, SteamIDs, ping, team, health in one reply.
		if ((curTime - m_LastG15Time) >= G15_INTERVAL)
		{
			if (QueueAction<GenericCommandAction>("g15_dumpplayer"))
				m_LastG15Time = curTime;
		}

		// `status` only when someone new shows up, plus a slow refresh for connection time/loss/state.
		const auto sinceStatus = curTime - m_LastStatusTime;
		if ((m_StatusRequested && sinceStatus >= G15_STATUS_MIN_INTERVAL) || sinceStatus >= G15_STATUS_REFRESH_INTERVAL)
		{
			if (QueueAction<GenericCommandAction>("status"))
			{
				m_LastStatusTime = curTime;
				m_StatusRequested = false;
			}
		}
	}
	else
	{
		// Legacy: alternate status / ping every 3 seconds.
		if ((curTime - m_LastStatusTime) >= LEGACY_STATUS_INTERVAL)
		{
			if (QueueAction<GenericCommandAction>(m_LegacyNextPing ? "ping" : "status"))
			{
				m_LastStatusTime = curTime;
				m_LegacyNextPing = !m_LegacyNextPing;
			}
		}
	}
}

void RCONActionManager::HandleG15Reply(const std::string& reply, time_point_t curTime)
{
	if (IsG15CommandUnavailable(reply))
	{
		if (!m_G15Unavailable)
			LogWarning("g15_dumpplayer is unavailable (TF2 was not launched with -g15). Falling back to status polling.");

		m_G15Unavailable = true;
		m_G15UnavailableSince = curTime;
		return;
	}

	const auto players = ParseG15Dump(reply);

	if (players.empty())
	{
		// An empty dump is normal in the main menu. But if `status` recently saw players and G15 keeps
		// coming back empty, the command isn't doing anything useful: fall back for a while.
		const bool statusSeesPlayers = (m_WorldState.GetCurrentTime() - m_WorldState.GetLastStatusUpdateTime()) < std::chrono::seconds(35);
		if (statusSeesPlayers && ++m_G15EmptyReplies >= 3)
		{
			LogWarning("g15_dumpplayer keeps returning no players while status does. Falling back to status polling for now.");
			m_G15Unavailable = true;
			m_G15UnavailableSince = curTime;
		}
		return;
	}

	m_G15EmptyReplies = 0;
	if (m_WorldState.ApplyG15Dump(players))
		m_StatusRequested = true;
}

void RCONActionManager::Update()
{
	ProcessQueuedCommands();
	ProcessRunningCommands();
}
