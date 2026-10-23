#include "Application.h"
#include "WebServer/WebUI.h"
#include "PreviewMode.h"
#include "DB/TempDB.h"

// pasted without looking
#include "DiscordRichPresence.h"
#include "Networking/GithubAPI.h"
#include "Networking/SteamAPI.h"
#include "ConsoleLog/NetworkStatus.h"
#include "Platform/Platform.h"
#include "Actions/ActionGenerators.h"
#include "BaseTextures.h"
#include "Filesystem.h"
#include "GenericErrors.h"
#include "Log.h"
#include "GameData/IPlayer.h"
#include "ReleaseChannel.h"
#include "TextureManager.h"
#include "UpdateManager.h"
#include "Util/PathUtils.h"
#include "Version.h"
#include "GlobalDispatcher.h"
#include "Networking/HTTPClient.h"

#include "UpdateManager.h"

#include "ConsoleLog/ConsoleLines/LobbyChangedLine.h"
#include "ConsoleLog/ConsoleLines/EdictUsageLine.h"

#include <Util/ScopeGuards.h>
#include <libzippp/libzippp.h>
#include <tf2pl/math/interpolation.hpp>
#include <tf2pl/text/case_insensitive_string.hpp>
#include <tf2pl/text/formatters/error_code.hpp>
#include <tf2pl/text/fmtstr.hpp>
#include <tf2pl/text/string_insertion.hpp>
#include <tf2pl/text/stringops.hpp>
#include <srcon/async_client.h>

#include <tf2pl/error/ensure.hpp>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <string>

using namespace tf2_bot_detector;
// 
using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;

static TF2BDApplication* s_Application;

namespace tf2_bot_detector
{
	tf2pl::dispatcher& GetDispatcher()
	{
		static tf2pl::dispatcher s_Dispatcher;
		return s_Dispatcher;
	}
}

TF2BDApplication::TF2BDApplication() :
	m_WorldState(IWorldState::Create(m_Settings)),
	m_ActionManager(RCONActionManager::Create(m_Settings, *m_WorldState)),
	m_UpdateManager(IUpdateManager::Create(m_Settings))
{
	assert(!s_Application);
	s_Application = this;

	m_TempDB = DB::ITempDB::Create();

	// moved from mainwindow
	ILogManager::GetInstance().CleanupLogFiles();

	m_WorldState->AddConsoleLineListener(this);
	m_WorldState->AddWorldEventListener(this);

	m_OpenTime = clock_t::now();

	// status/ping/g15_dumpplayer polling is handled by RCONActionManager (see UpdatePlayerDataPolling)
	GetActionManager().AddPeriodicActionGenerator<ConfigActionGenerator>();
	GetActionManager().AddPeriodicActionGenerator<LobbyDebugActionGenerator>(m_Settings);

	// always run at first tick.
	QueueUpdate();
}

TF2BDApplication::~TF2BDApplication() = default;

TF2BDApplication& TF2BDApplication::GetApplication()
{
	return *mh_ensure(s_Application);
}

DB::ITempDB& TF2BDApplication::GetTempDB()
{
	return *mh_ensure(m_TempDB.get());
}

void TF2BDApplication::OnConsoleLineParsed(IWorldState& world, IConsoleLine& parsed)
{
	m_ParsedLineCount++;

	if (parsed.ShouldPrint() && m_MainState)
	{
		while (m_MainState->m_PrintingLines.size() > m_MainState->MAX_PRINTING_LINES)
			m_MainState->m_PrintingLines.pop_back();

		m_MainState->m_PrintingLines.push_front(parsed.shared_from_this());
	}

	switch (parsed.GetType())
	{
	case ConsoleLineType::LobbyChanged:
	{
		auto& lobbyChangedLine = static_cast<const LobbyChangedLine&>(parsed);
		const LobbyChangeType changeType = lobbyChangedLine.GetChangeType();

		if (changeType == LobbyChangeType::Created || changeType == LobbyChangeType::Updated)
			GetActionManager().QueueAction<LobbyUpdateAction>();

		break;
	}
	case ConsoleLineType::EdictUsage:
	{
		auto& usageLine = static_cast<const EdictUsageLine&>(parsed);
		m_EdictUsageSamples.push_back({ usageLine.GetTimestamp(), usageLine.GetUsedEdicts(), usageLine.GetTotalEdicts() });

		while (m_EdictUsageSamples.front().m_Timestamp < (usageLine.GetTimestamp() - 5min))
			m_EdictUsageSamples.erase(m_EdictUsageSamples.begin());

		break;
	}

	default: break;
	}
}

void TF2BDApplication::OnConsoleLineUnparsed(IWorldState& world, const std::string_view& text)
{
	// why does it add when it's called unparsed wtf
	m_ParsedLineCount++;
}

void TF2BDApplication::OnConsoleLogChunkParsed(IWorldState& world, bool consoleLinesUpdated)
{
	assert(&world == m_WorldState.get());

	if (consoleLinesUpdated)
		UpdateServerPing(GetCurrentTimestampCompensated());
}

bool TF2BDApplication::IsTimeEven() const
{
	const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(clock_t::now() - m_OpenTime);
	return !(seconds.count() % 2);
}

float TF2BDApplication::TimeSine(float interval, float min, float max) const
{
	const auto elapsed = (clock_t::now() - m_OpenTime) % std::chrono::duration_cast<clock_t::duration>(std::chrono::duration<float>(interval));
	const auto progress = std::chrono::duration<float>(elapsed).count() / interval;
	return tf2pl::remap(std::sin(progress * 6.28318530717958647693f), -1.0f, 1.0f, min, max);
}

time_point_t TF2BDApplication::GetCurrentTimestampCompensated() const
{
	return GetWorld().GetCurrentTime();
}

void TF2BDApplication::UpdateServerPing(time_point_t timestamp)
{
	if ((timestamp - m_LastServerPingSample) <= 7s)
		return;

	float totalPing = 0;
	uint16_t samples = 0;

	for (IPlayer& player : m_WorldState->GetPlayers())
	{
		if (player.GetLastStatusUpdateTime() < (timestamp - 20s))
			continue;

		auto& data = player.GetOrCreateData<PlayerExtraData>(player);
		totalPing += data.GetAveragePing();
		samples++;
	}

	m_ServerPingSamples.push_back({ timestamp, uint16_t(totalPing / samples) });
	m_LastServerPingSample = timestamp;

	while ((timestamp - m_ServerPingSamples.front().m_Timestamp) > 5min)
		m_ServerPingSamples.erase(m_ServerPingSamples.begin());
}

void tf2_bot_detector::TF2BDApplication::Update()
{
	if (m_Paused)
		return;

	GetDispatcher().run_for(10ms);

	m_WorldState->Update();
	m_UpdateManager->Update();
	CollectUntrustedListReport();

	if (m_Settings.m_Unsaved.m_RCONClient)
		m_Settings.m_Unsaved.m_RCONClient->set_logging(m_Settings.m_Logging.m_RCONPackets);

	m_SetupFlow.SetPreviewBypass(m_PreviewRequested);
	if (m_SetupFlow.OnUpdate(m_Settings))
	{
		m_Preview.reset(); // uses m_MainState's moderator logic
		m_MainState.reset();
	}
	else
	{
		if (!m_MainState)
			m_MainState.emplace(*this);

		if (m_PreviewRequested && !m_Preview)
			m_Preview = std::make_unique<PreviewSession>(*m_MainState->m_ModeratorLogic, m_Settings);
		else if (!m_PreviewRequested && m_Preview)
			m_Preview.reset();

		if (m_Preview)
		{
			// the game side stays idle while previewing: nothing new is parsed and nothing is acted on
			m_Preview->Update();
		}
		else
		{
			m_MainState->m_Parser.Update();
			m_MainState->m_ModeratorLogic->Update();
		}

		m_MainState->OnUpdateDiscord();
	}

	if (m_Settings.IsWebUIMode() && m_MainState)
	{
		if (!m_WebUI)
			m_WebUI = std::make_unique<WebUIController>(*this);
		m_WebUI->Update();
	}
	else if (m_WebUI)
	{
		m_WebUI.reset(); // switched back to native (or back into setup): stop serving
	}

	GetActionManager().Update();

	// set our "should update even tabbed out" variable to false
	if (b_ShouldUpdate) {
		b_ShouldUpdate = false;
	}
}

float TF2BDApplication::PlayerExtraData::GetAveragePing() const
{
	unsigned totalPing = m_Parent->GetPing();
	unsigned samples = 1;

	for (const auto& entry : m_PingHistory)
	{
		totalPing += entry.m_Ping;
		samples++;
	}

	return totalPing / float(samples);
}

TF2BDApplication::PostSetupFlowState::PostSetupFlowState(TF2BDApplication& app) :
	m_Parent(&app),
	m_ModeratorLogic(IModeratorLogic::Create(*app.m_WorldState, app.m_Settings, app.GetActionManager())),
	m_Parser(*app.m_WorldState, app.m_Settings, app.m_Settings.GetTFDir() / "console.log")
{
#ifdef TF2BD_ENABLE_DISCORD_INTEGRATION
	m_DRPManager = IDRPManager::Create(app.m_Settings, *app.m_WorldState);
#endif
}

void TF2BDApplication::PostSetupFlowState::OnUpdateDiscord()
{
#ifdef TF2BD_ENABLE_DISCORD_INTEGRATION
	const auto curTime = clock_t::now();
	if (!m_DRPManager && m_Parent->m_Settings.m_Discord.m_EnableRichPresence)
	{
		m_DRPManager = IDRPManager::Create(m_Parent->m_Settings, *m_Parent->m_WorldState);
	}
	else if (m_DRPManager && !m_Parent->m_Settings.m_Discord.m_EnableRichPresence)
	{
		m_DRPManager.reset();
	}

	if (m_DRPManager)
		m_DRPManager->Update();
#endif
}

tf2pl::generator<IPlayer&> TF2BDApplication::PostSetupFlowState::GeneratePlayerPrintData()
{
	IPlayer* printData[33]{};
	auto begin = std::begin(printData);
	auto end = std::end(printData);
	assert(begin <= end);
	IWorldState* world = &m_Parent->GetWorld(); // the sample lobby in preview mode
	assert(static_cast<size_t>(end - begin) >= world->GetApproxLobbyMemberCount());

	std::fill(begin, end, nullptr);

	{
		auto* current = begin;
		for (IPlayer& member : world->GetLobbyMembers())
		{
			*current = &member;
			current++;
		}

		if (current == begin)
		{
			// We seem to have either an empty lobby or we're playing on a community server.
			// Just find the most recent status updates.
			for (IPlayer& playerData : world->GetPlayers())
			{
				if (playerData.GetLastStatusUpdateTime() >= (world->GetLastStatusUpdateTime() - 15s))
				{
					*current = &playerData;
					current++;

					if (current >= end)
						break; // This might happen, but we're not in a lobby so everything has to be approximate
				}
			}
		}

		end = current;
	}

	std::sort(begin, end, [](const IPlayer* lhs, const IPlayer* rhs) -> bool
		{
			assert(lhs);
			assert(rhs);
			//if (!lhs && !rhs)
			//	return false;
			//if (auto result = !!rhs <=> !!lhs; !std::is_eq(result))
			//	return result < 0;

			// Intentionally reversed, we want descending kill order
			if (auto killsResult = rhs->GetScores().m_Kills <=> lhs->GetScores().m_Kills; !std::is_eq(killsResult))
				return std::is_lt(killsResult);

			if (auto deathsResult = lhs->GetScores().m_Deaths <=> rhs->GetScores().m_Deaths; !std::is_eq(deathsResult))
				return std::is_lt(deathsResult);

			// Sort by ascending userid
			{
				auto luid = lhs->GetUserID();
				auto ruid = rhs->GetUserID();
				if (luid && ruid)
				{
					if (auto result = *luid <=> *ruid; !std::is_eq(result))
						return std::is_lt(result);
				}
			}

			return false;
		});

	for (auto it = begin; it != end; ++it)
		co_yield **it;
}


IModeratorLogic& TF2BDApplication::GetModLogic()
{
	if (m_Preview)
		return m_Preview->GetModLogic();
	return *m_MainState.value().m_ModeratorLogic;
}

const IModeratorLogic& TF2BDApplication::GetModLogic() const
{
	if (m_Preview)
		return m_Preview->GetModLogic();
	return *m_MainState.value().m_ModeratorLogic;
}

IWorldState& TF2BDApplication::GetWorld()
{
	if (m_Preview)
		return m_Preview->GetWorld();
	return *m_WorldState;
}

const IWorldState& TF2BDApplication::GetWorld() const
{
	if (m_Preview)
		return m_Preview->GetWorld();
	return *m_WorldState;
}

void TF2BDApplication::SetPreviewMode(bool enabled)
{
	if (m_PreviewRequested == enabled)
		return;

	m_PreviewRequested = enabled;
	Log(enabled ? "Starting preview mode..." : "Leaving preview mode...");
	QueueUpdate();
}

bool TF2BDApplication::CanStartPreview() const
{
	return m_SetupFlow.CanPreviewBypass(m_Settings);
}

PlayerAttributesList TF2BDApplication::GetEditablePlayerAttributes(const SteamID& id)
{
	if (m_Preview && m_Preview->OwnsPlayer(id))
		return m_Preview->GetEditableAttributes(id);

	if (!m_MainState)
		return {};

	if (auto* playerList = m_MainState->m_ModeratorLogic->GetPlayerList())
		return playerList->GetEditablePlayerAttributes(id);

	return {};
}

const std::list<std::shared_ptr<const IConsoleLine>>& TF2BDApplication::GetDisplayedConsoleLines() const
{
	static const std::list<std::shared_ptr<const IConsoleLine>> s_Empty;
	if (m_Preview)
		return m_Preview->GetConsoleLines();
	if (m_MainState)
		return m_MainState->m_PrintingLines;
	return s_Empty;
}

void TF2BDApplication::CollectUntrustedListReport()
{
	// the list loader can screen more than once (e.g. Reload Playerlists); merge anything new
	if (!UntrustedLists::HasReport())
		return;

	auto report = UntrustedLists::TakeReport();

	const auto IsAcknowledged = [&](const std::filesystem::path& path)
	{
		const auto name = path.filename().string();
		const auto& acked = m_Settings.m_AcknowledgedUntrustedLists;
		return std::find(acked.begin(), acked.end(), name) != acked.end();
	};
	std::erase_if(report.m_Contaminated, IsAcknowledged);
	std::erase_if(report.m_PossiblyContaminatedExports, IsAcknowledged);

	const auto Merge = [](std::vector<std::filesystem::path>& into, const std::vector<std::filesystem::path>& from)
	{
		for (const auto& p : from)
			if (std::find(into.begin(), into.end(), p) == into.end())
				into.push_back(p);
	};

	// blacklisted findings are never filtered by acknowledgements
	auto& pending = m_UntrustedListReport;
	Merge(pending.m_BlacklistedDeleted, report.m_BlacklistedDeleted);
	Merge(pending.m_BlacklistedDisabled, report.m_BlacklistedDisabled);
	Merge(pending.m_BlacklistedKept, report.m_BlacklistedKept);
	Merge(pending.m_Deleted, report.m_Deleted);
	Merge(pending.m_Contaminated, report.m_Contaminated);
	Merge(pending.m_PossiblyContaminatedExports, report.m_PossiblyContaminatedExports);
}

void TF2BDApplication::DismissUntrustedListReport(bool acknowledge)
{
	if (acknowledge)
	{
		auto& acked = m_Settings.m_AcknowledgedUntrustedLists;
		for (const auto* files : { &m_UntrustedListReport.m_Contaminated, &m_UntrustedListReport.m_PossiblyContaminatedExports })
		{
			for (const auto& file : *files)
			{
				const auto name = file.filename().string();
				if (std::find(acked.begin(), acked.end(), name) == acked.end())
					acked.push_back(name);
			}
		}
		m_Settings.SaveFile();
	}

	m_UntrustedListReport = {};
}

bool TF2BDApplication::IsSleepingEnabled() const
{
	return m_Settings.m_SleepWhenUnfocused;
}

/// <summary>
/// force our program to update, even if sleeping is enabled and our application is sleeping,
/// aka wake it up
/// </summary>
/// <returns></returns>
void TF2BDApplication::QueueUpdate()
{
	b_ShouldUpdate = true;
}

/// <summary>
/// should we run Update()?
/// </summary>
/// <returns></returns>
bool TF2BDApplication::ShouldUpdate()
{
	// an open web UI page keeps the app awake, so it stays as live as the native window
	return !this->IsSleepingEnabled() || b_ShouldUpdate || (m_WebUI && m_WebUI->GetLiveClientCount() > 0);
}
