#include "PlayerListExport.h"

#include "Config/ConfigHelpers.h"
#include "Filesystem.h"
#include "Log.h"
#include "Networking/HTTPClient.h"
#include "Networking/SteamAPI.h"
#include "Networking/SteamHistoryAPI.h"

#include <mh/text/fmtstr.hpp>
#include <mh/text/string_insertion.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string_view>
#include <unordered_set>
#include <utility>

using namespace tf2_bot_detector;
using namespace std::string_literals;
using namespace std::string_view_literals;

namespace tf2_bot_detector
{
	namespace
	{
		constexpr size_t EXPORT_BATCH_SIZE = 100;
		constexpr int EXPORT_SCHEMA_VERSION = 3;

		std::vector<SteamID> CollectSteamIDs(const std::map<SteamID, PlayerListData>& players)
		{
			std::vector<SteamID> ids;
			ids.reserve(players.size());
			for (const auto& entry : players)
				ids.push_back(entry.first);
			return ids;
		}

		std::shared_ptr<const IHTTPClient> GetRequiredHTTPClient(const Settings& settings)
		{
			if (!settings.IsSteamAPIAvailable())
				throw std::runtime_error("The Steam API is not available; set a Steam API key in Settings -> Service Integrations.");

			if (auto client = settings.GetHTTPClient())
				return client;

			throw std::runtime_error("Internet usage is disabled in settings, so the export cannot contact the Steam APIs.");
		}

		/// <summary>
		/// Serializes a PlayerListData entry exactly like the playerlist schema expects
		/// (mirrors the attribute name mappings used when loading playerlists).
		/// </summary>
		void SerializePlayerData(nlohmann::json& j, const PlayerListData& data)
		{
			const auto SerializeAttributes = [](const PlayerAttributesList& attrs)
			{
				nlohmann::json attrJson = nlohmann::json::array();

				const auto PushAttribute = [&attrJson, &attrs](PlayerAttribute attr, const char* name)
				{
					if (attrs.HasAttribute(attr))
						attrJson.push_back(name);
				};

				PushAttribute(PlayerAttribute::Cheater, "cheater");
				PushAttribute(PlayerAttribute::Suspicious, "suspicious");
				PushAttribute(PlayerAttribute::Exploiter, "exploiter");
				PushAttribute(PlayerAttribute::Racist, "racist");
				PushAttribute(PlayerAttribute::SuspectedCheater, "suspected_cheater");
				PushAttribute(PlayerAttribute::Blacklisted, "blacklisted");
				PushAttribute(PlayerAttribute::VACBanned, "vac_banned");
				PushAttribute(PlayerAttribute::GameBanned, "game_banned");
				PushAttribute(PlayerAttribute::SourceBanned, "sourcebanned");
				PushAttribute(PlayerAttribute::Pedophilia, "pedophilia");

				for (const auto& tag : attrs.GetCustomTags())
					attrJson.push_back(tag);

				return attrJson;
			};

			j = nlohmann::json
			{
				{ "steamid", data.GetSteamID() },
				{ "attributes", SerializeAttributes(data.m_SavedAttributes) }
			};

			if (data.m_LastSeen)
			{
				nlohmann::json& lastSeen = j["last_seen"];
				if (!data.m_LastSeen->m_PlayerName.empty())
					lastSeen["player_name"] = data.m_LastSeen->m_PlayerName;
				lastSeen["time"] = std::chrono::duration_cast<std::chrono::seconds>(
					data.m_LastSeen->m_Time.time_since_epoch()).count();
			}

			if (!data.m_Proof.empty())
				j["proof"] = data.m_Proof;
		}
	}
}

PlayerListExporter::PlayerListExporter(const Settings& settings, PlayerListJSON& sourceList) :
	m_Settings(settings), m_PlayerList(sourceList)
{
}

PlayerListExporter::~PlayerListExporter()
{
	m_Cancel = true;
	if (m_Thread.joinable())
		m_Thread.join();
}

void PlayerListExporter::Start()
{
	bool expected = false;
	if (!m_Running.compare_exchange_strong(expected, true))
		return;

	if (m_Thread.joinable())
		m_Thread.join();

	m_Cancel = false;
	m_Thread = std::thread([this]()
		{
			RunExport();
			m_Running = false;
		});
}

void PlayerListExporter::Cancel()
{
	m_Cancel = true;
}

bool PlayerListExporter::IsRunning() const
{
	return m_Running;
}

PlayerListExporter::Progress PlayerListExporter::GetProgress() const
{
	std::lock_guard lock(m_ProgressMutex);
	return m_Progress;
}

void PlayerListExporter::PublishProgress()
{
	std::lock_guard lock(m_ProgressMutex);
	m_Progress = m_Worker;
}

void PlayerListExporter::SetStage(Progress::Stage stage, std::string message)
{
	m_Worker.m_Stage = stage;
	m_Worker.m_Message = std::move(message);
	PublishProgress();
}

void PlayerListExporter::SetProgress(Progress::Stage stage, size_t completed, size_t total, std::string message)
{
	m_Worker.m_Stage = stage;
	m_Worker.m_Completed = completed;
	m_Worker.m_Total = total;
	m_Worker.m_Message = std::move(message);
	PublishProgress();
}

void PlayerListExporter::RunExport()
{
	m_Players.clear();
	m_Worker = Progress{};

	try
	{
		SetProgress(Progress::Stage::Gathering, 0, 0, "Gathering players from all loaded playerlists...");
		GatherPlayers();

		if (m_Cancel)
			return;

		Log("Total Export: gathered {} players.", m_Players.size());
		if (m_Players.empty())
		{
			SetStage(Progress::Stage::Finished, "No players were found in any loaded playerlist.");
			return;
		}

		SetProgress(Progress::Stage::Summaries, 0, m_Players.size(),
			"Refreshing player summaries ("s << m_Players.size() << " players)...");
		RefreshSummaries();

		if (m_Cancel)
			return;

		SetProgress(Progress::Stage::Bans, 0, m_Players.size(), m_Worker.m_Message);
		RefreshBans();

		if (m_Cancel)
			return;

		SetProgress(Progress::Stage::SourceBans, 0, m_Players.size(), m_Worker.m_Message);
		RefreshSourceBans();

		if (m_Cancel)
			return;

		SetStage(Progress::Stage::Writing, "Writing cfg/playerlist.export.json...");
		WriteFile();

		if (m_Cancel)
			return;

		const size_t finalCount = m_Players.size();
		SetProgress(Progress::Stage::Finished, finalCount, finalCount,
			"Done! Exported "s << finalCount << " players to cfg/playerlist.export.json.");
		Log("Total Export: exported {} players.", finalCount);
	}
	catch (...)
	{
		LogException(MH_SOURCE_LOCATION_CURRENT(), "Total Export failed");

		SetStage(Progress::Stage::Failed, "Export failed. Check the app log for details.");
	}

	if (m_Cancel && m_Worker.m_Stage != Progress::Stage::Finished)
		SetStage(Progress::Stage::Failed, "Export cancelled before it completed.");
}

void PlayerListExporter::GatherPlayers()
{
	for (const auto& [fileName, data] : m_PlayerList.GetAllPlayerData())
	{
		(void)fileName;

		auto& out = m_Players.try_emplace(data.GetSteamID(), data.GetSteamID()).first->second;

		out.m_SavedAttributes |= data.m_SavedAttributes;
		out.m_LastSeen = PlayerListData::LastSeen::Latest(out.m_LastSeen, data.m_LastSeen);

		for (const auto& proof : data.m_Proof)
		{
			if (std::find(out.m_Proof.begin(), out.m_Proof.end(), proof) == out.m_Proof.end())
				out.m_Proof.push_back(proof);
		}
	}

	m_Worker.m_Total = m_Players.size();
}

void PlayerListExporter::RefreshSummaries()
{
	const auto client = GetRequiredHTTPClient(m_Settings);

	std::vector<SteamID> nonexistent;
	const auto allIDs = CollectSteamIDs(m_Players);
	const size_t total = allIDs.size();

	for (size_t offset = 0; offset < total && !m_Cancel; offset += EXPORT_BATCH_SIZE)
	{
		const size_t batchEnd = std::min(offset + EXPORT_BATCH_SIZE, total);
		const std::vector<SteamID> batch(allIDs.begin() + offset, allIDs.begin() + batchEnd);

		const auto summaries = SteamAPI::GetPlayerSummariesAsync(m_Settings, batch, *client).get();

		std::unordered_set<SteamID> returned;
		returned.reserve(summaries.size());
		for (const auto& summary : summaries)
		{
			returned.insert(summary.m_SteamID);

			auto found = m_Players.find(summary.m_SteamID);
			if (found == m_Players.end())
				continue;

			// Fill in a logged name when we still don't have one for this profile.
			if ((!found->second.m_LastSeen || found->second.m_LastSeen->m_PlayerName.empty()) && !summary.m_Nickname.empty())
			{
				found->second.m_LastSeen = PlayerListData::LastSeen{ std::chrono::system_clock::now(), summary.m_Nickname };
				m_Worker.m_FilledNames++;
			}
		}

		// Accounts that GetPlayerSummaries doesn't return are confirmed nonexistent.
		for (const auto& id : batch)
		{
			if (!returned.contains(id))
				nonexistent.push_back(id);
		}

		SetProgress(Progress::Stage::Summaries, offset + batch.size(), total,
			mh::fmtstr<128>("Fetching player summaries... {} / {}", offset + batch.size(), total).c_str());
	}

	if (!nonexistent.empty())
	{
		for (const auto& id : nonexistent)
			m_Players.erase(id);
	}

	m_Worker.m_RemovedNonexistent += nonexistent.size();
	SetProgress(Progress::Stage::Summaries, total, total,
		mh::fmtstr<192>("Removed {} nonexistent accounts, filled in {} names.", nonexistent.size(), m_Worker.m_FilledNames).c_str());
}

void PlayerListExporter::RefreshBans()
{
	const auto client = GetRequiredHTTPClient(m_Settings);

	const auto allIDs = CollectSteamIDs(m_Players);
	const size_t total = allIDs.size();

	for (size_t offset = 0; offset < total && !m_Cancel; offset += EXPORT_BATCH_SIZE)
	{
		const size_t batchEnd = std::min(offset + EXPORT_BATCH_SIZE, total);
		const std::vector<SteamID> batch(allIDs.begin() + offset, allIDs.begin() + batchEnd);

		const auto bans = SteamAPI::GetPlayerBansAsync(m_Settings, batch, *client).get();

		for (const auto& playerBans : bans)
		{
			auto found = m_Players.find(playerBans.m_SteamID);
			if (found == m_Players.end())
				continue;

			const bool vacBanned = playerBans.m_VACBanCount > 0;
			const bool gameBanned = playerBans.m_GameBanCount > 0;

			found->second.m_SavedAttributes.SetAttribute(PlayerAttribute::VACBanned, vacBanned);
			found->second.m_SavedAttributes.SetAttribute(PlayerAttribute::GameBanned, gameBanned);

			if (vacBanned) m_Worker.m_VACBanned++;
			if (gameBanned) m_Worker.m_GameBanned++;
		}

		SetProgress(Progress::Stage::Bans, offset + batch.size(), total,
			mh::fmtstr<128>("Refreshing ban info... {} / {}", offset + batch.size(), total).c_str());
	}

	SetProgress(Progress::Stage::Bans, total, total,
		mh::fmtstr<192>("Refreshed ban info: {} VAC bans, {} game bans.", m_Worker.m_VACBanned, m_Worker.m_GameBanned).c_str());
}

void PlayerListExporter::RefreshSourceBans()
{
	const bool integrationEnabled =
		m_Settings.m_AllowInternetUsage.value_or(false) &&
		m_Settings.m_EnableSteamHistoryIntegration &&
		!m_Settings.GetSteamHistoryAPIKey().empty();

	if (!integrationEnabled)
	{
		LogWarning("Total Export: SteamHistory integration is disabled, skipping SourceBan refresh.");
		SetStage(Progress::Stage::SourceBans,
			"SteamHistory integration is disabled; SourceBan markers were left unchanged.");
		return;
	}

	const auto client = GetRequiredHTTPClient(m_Settings);

	const auto allIDs = CollectSteamIDs(m_Players);
	const size_t total = allIDs.size();

	for (size_t offset = 0; offset < total && !m_Cancel; offset += EXPORT_BATCH_SIZE)
	{
		const size_t batchEnd = std::min(offset + EXPORT_BATCH_SIZE, total);
		const std::vector<SteamID> batch(allIDs.begin() + offset, allIDs.begin() + batchEnd);

		const auto response = SteamHistoryAPI::GetPlayerSourceBansAsync(m_Settings.GetSteamHistoryAPIKey(), batch, *client).get();

		for (const auto& id : batch)
		{
			auto found = m_Players.find(id);
			if (found == m_Players.end())
				continue;

			const auto responseFound = response.find(id);
			const bool hasBans = (responseFound != response.end()) && !responseFound->second.empty();
			if (hasBans) m_Worker.m_SourceBanned++;

			found->second.m_SavedAttributes.SetAttribute(PlayerAttribute::SourceBanned, hasBans);
		}

		SetProgress(Progress::Stage::SourceBans, offset + batch.size(), total,
			mh::fmtstr<128>("Refreshing SourceBans... {} / {}", offset + batch.size(), total).c_str());
	}

	SetProgress(Progress::Stage::SourceBans, total, total,
		mh::fmtstr<192>("Refreshed SourceBans: {} SourceBan records.", m_Worker.m_SourceBanned).c_str());
}

void PlayerListExporter::WriteFile() const
{
	nlohmann::json json;
	json["$schema"] = ConfigSchemaInfo("playerlist", EXPORT_SCHEMA_VERSION);

	ConfigFileInfo fileInfo;
	fileInfo.m_Authors = { "Total Export" };
	fileInfo.m_Title = "Total Export - All Loaded Playerlists";
	fileInfo.m_Description =
		"Automatically merged list of every player across all loaded playerlists. "
		"Names, VAC/game bans and SourceBans were refreshed from the APIs at export time.";
	json["file_info"] = fileInfo;

	auto& players = json["players"];
	players = nlohmann::json::array();

	for (const auto& [id, data] : m_Players)
	{
		if (data.m_SavedAttributes.empty())
			continue;

		nlohmann::json entry;
		SerializePlayerData(entry, data);
		players.push_back(std::move(entry));
	}

	const std::filesystem::path path("cfg/playerlist.export.json");
	const std::string content = json.dump(1, '\t', true, nlohmann::detail::error_handler_t::ignore) + '\n';

	IFilesystem::Get().WriteFile(path, content, PathUsage::WriteRoaming);
	Log("Total Export: wrote {} players to {}", players.size(), path.string());
}