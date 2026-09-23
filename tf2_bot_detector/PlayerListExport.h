#pragma once

#include "Config/PlayerListJSON.h"
#include "Config/Settings.h"
#include "SteamID.h"

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace tf2_bot_detector
{
	/// <summary>
	/// Combines every loaded playerlist into a single merged list
	/// (TF2PL and TF2BD-compatible schema-v3 files). Fills in missing names, refreshes
	/// VAC/game/SourceBan markers from the APIs, and drops accounts that no longer
	/// exist on Steam. Runs on a background thread; the UI polls GetProgress().
	/// </summary>
	class PlayerListExporter final
	{
	public:
		PlayerListExporter(const Settings& settings, PlayerListJSON& sourceList);
		~PlayerListExporter();

		PlayerListExporter(const PlayerListExporter&) = delete;
		PlayerListExporter& operator=(const PlayerListExporter&) = delete;

		/// <summary>
		/// Launches the export on a background thread. Safe to call again after a
		/// previous run has finished to re-run the export.
		/// </summary>
		void Start();
		/// <summary>
		/// Requests the export to stop after the current API batch completes.
		/// </summary>
		void Cancel();

		bool IsRunning() const;

		struct Progress
		{
			enum class Stage
			{
				Idle,
				Gathering,
				Summaries,
				Bans,
				SourceBans,
				Writing,
				Finished,
				Failed,
			};

			Stage m_Stage = Stage::Idle;
			size_t m_Completed = 0;
			size_t m_Total = 0;
			std::string m_Message;

			size_t m_RemovedNonexistent = 0;
			size_t m_FilledNames = 0;
			size_t m_VACBanned = 0;
			size_t m_GameBanned = 0;
			size_t m_SourceBanned = 0;
		};
		Progress GetProgress() const;

	private:
		using PlayerMap_t = std::map<SteamID, PlayerListData>;

		void RunExport();

		void GatherPlayers();
		void RefreshSummaries();
		void RefreshBans();
		void RefreshSourceBans();
		void WriteFiles() const;

		void SetStage(Progress::Stage stage, std::string message);
		void SetProgress(Progress::Stage stage, size_t completed, size_t total, std::string message);
		void PublishProgress();

		const Settings& m_Settings;
		PlayerListJSON& m_PlayerList;

		// Only touched from the worker thread once Start() has been called.
		PlayerMap_t m_Players;
		Progress m_Worker;

		mutable std::mutex m_ProgressMutex;
		Progress m_Progress;
		std::atomic<bool> m_Running{ false };
		std::atomic<bool> m_Cancel{ false };
		std::thread m_Thread;
	};
}
