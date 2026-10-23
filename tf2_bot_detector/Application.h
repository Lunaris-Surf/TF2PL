#pragma once

#include "Actions/Actions.h"
#include "Actions/RCONActionManager.h"
#include "Clock.h"
#include "CompensatedTS.h"
#include "ConsoleLog/ConsoleLineListener.h"
#include "ConsoleLog/ConsoleLogParser.h"
#include "Config/PlayerListJSON.h"
#include "Config/Settings.h"
#include "DiscordRichPresence.h"
#include "Networking/GithubAPI.h"
#include "ModeratorLogic.h"
#include "SetupFlow/SetupFlow.h"
#include "WorldEventListener.h"
#include "WorldState.h"
#include "LobbyMember.h"
#include "PlayerStatus.h"
#include "GameData/TFConstants.h"
#include "Config/UntrustedLists.h"

#include <tf2pl/error/expected.hpp>

#include <optional>
#include <vector>
#include <memory>

namespace tf2_bot_detector
{
	class IBaseTextures;
	class IConsoleLine;
	class IConsoleLineListener;
	class ITexture;
	class ITextureManager;
	class IUpdateManager;
	class PreviewSession;
	class WebUIController;

	/// zips the logs folder into debug_report.zip and shows it in the file browser (File > Generate Debug Report)
	void GenerateDebugReport();

	namespace DB
	{
		class ITempDB;
	}

	class TF2BDApplication : IConsoleLineListener, BaseWorldEventListener
	{
	public:
		TF2BDApplication();
		~TF2BDApplication();

		static TF2BDApplication& GetApplication();
		DB::ITempDB& GetTempDB();
		Settings& GetSettings() { return m_Settings; }
		const Settings& GetSettings() const { return m_Settings; }

	private:
		std::unique_ptr<DB::ITempDB> m_TempDB;

		// moved from "MainWindow"
		const void* m_LastLogMessage = nullptr;

		bool IsSleepingEnabled() const;

		bool IsTimeEven() const;
		float TimeSine(float interval = 1.0f, float min = 0, float max = 1) const;

		// IConsoleLineListener
		void OnConsoleLineParsed(IWorldState& world, IConsoleLine& line) override;
		void OnConsoleLineUnparsed(IWorldState& world, const std::string_view& text) override;
		void OnConsoleLogChunkParsed(IWorldState& world, bool consoleLinesParsed) override;
		size_t m_ParsedLineCount = 0;

		// IWorldEventListener
		// void OnChatMsg(WorldState& world, const IPlayer& player, const std::string_view& msg) override;
		// void OnUpdate(WorldState& world, bool consoleLinesUpdated) override;

		/// <summary>
		/// is our application paused?
		/// </summary>
		bool m_Paused = false;

		// Gets the current timestamp, but time progresses in real time even without new messages
		time_point_t GetCurrentTimestampCompensated() const;

		struct PingSample
		{
			constexpr PingSample(time_point_t timestamp, uint16_t ping) :
				m_Timestamp(timestamp), m_Ping(ping)
			{
			}

			time_point_t m_Timestamp{};
			uint16_t m_Ping{};
		};

		struct PlayerExtraData final
		{
			PlayerExtraData(const IPlayer& player) : m_Parent(&player) {}

			const IPlayer* m_Parent = nullptr;

			std::string m_pendingReason;

			time_point_t m_LastPingUpdateTime{};
			std::vector<PingSample> m_PingHistory{};
			float GetAveragePing() const;
		};

		struct EdictUsageSample
		{
			time_point_t m_Timestamp;
			uint16_t m_UsedEdicts;
			uint16_t m_MaxEdicts;
		};
		std::vector<EdictUsageSample> m_EdictUsageSamples;

		time_point_t m_OpenTime;

		std::vector<PingSample> m_ServerPingSamples;
		time_point_t m_LastServerPingSample{};
		void UpdateServerPing(time_point_t timestamp);

		Settings m_Settings;

		SetupFlow m_SetupFlow;

		std::unique_ptr<IUpdateManager> m_UpdateManager;
		std::shared_ptr<IWorldState> m_WorldState;
		std::unique_ptr<RCONActionManager> m_ActionManager;

		/// <summary>
		/// for "sleep when unfocused" feature.
		/// note: it still will update, when a new log as been processed.
		/// </summary>
		bool b_ShouldUpdate = false;

		struct PostSetupFlowState
		{
			PostSetupFlowState(TF2BDApplication& window);

			TF2BDApplication* m_Parent = nullptr;
			std::unique_ptr<IModeratorLogic> m_ModeratorLogic;

			ConsoleLogParser m_Parser;
			std::list<std::shared_ptr<const IConsoleLine>> m_PrintingLines;  // newest to oldest order
			static constexpr size_t MAX_PRINTING_LINES = 512;
			tf2pl::generator<IPlayer&> GeneratePlayerPrintData();

			void OnUpdateDiscord();
#ifdef TF2BD_ENABLE_DISCORD_INTEGRATION
			std::unique_ptr<IDRPManager> m_DRPManager;
#endif
		};

		std::optional<PostSetupFlowState> m_MainState;

		// preview mode (sample lobby instead of TF2); declared after m_MainState, it uses its moderator logic
		bool m_PreviewRequested = false;
		std::unique_ptr<PreviewSession> m_Preview;

		// untrusted/blacklisted list findings waiting to be acknowledged (shown by the native and web UIs)
		UntrustedLists::Report m_UntrustedListReport;
		void CollectUntrustedListReport();

		// optional browser front end (Settings > UI > Interface = Web); declared after m_MainState so it shuts down first
		std::unique_ptr<WebUIController> m_WebUI;

	public:
		/// null unless the web UI is enabled and the app is past the setup flow
		WebUIController* GetWebUI() { return m_WebUI.get(); }

		void QueueUpdate();
		bool ShouldUpdate();
		void Update();

		std::optional<PostSetupFlowState>& GetMainState() { return m_MainState; }

		/// <summary>
		/// get moderation logic. In preview mode this is the preview session's (sample players' marks live in
		/// memory; everyone else is passed through to the real one).
		/// </summary>
		IModeratorLogic& GetModLogic();
		const IModeratorLogic& GetModLogic() const;

		/// <summary>
		/// get the world state the UI shows: the sample lobby in preview mode, otherwise the game's.
		/// </summary>
		IWorldState& GetWorld();
		const IWorldState& GetWorld() const;

		/// the game's world state, even in preview mode
		IWorldState& GetGameWorld() { return *m_WorldState; }

		/// <summary>
		/// Preview mode: explore TF2PL with a sample lobby while TF2 isn't running. Takes effect on the next update.
		/// </summary>
		void SetPreviewMode(bool enabled);
		bool IsPreviewRequested() const { return m_PreviewRequested; }
		bool IsPreviewMode() const { return m_Preview != nullptr; }
		PreviewSession* GetPreview() { return m_Preview.get(); }
		/// true while the setup flow only waits for Steam/TF2, i.e. when preview mode would let you in
		bool CanStartPreview() const;

		/// marks the "your list" editors show for a player (sample players' in-memory marks in preview mode)
		PlayerAttributesList GetEditablePlayerAttributes(const SteamID& id);

		/// chat/console lines for the chat pane, newest first (sample chat in preview mode). Empty before setup.
		const std::list<std::shared_ptr<const IConsoleLine>>& GetDisplayedConsoleLines() const;

		bool IsPaused() const { return m_Paused; }
		void SetPaused(bool paused) { m_Paused = paused; QueueUpdate(); }

		IUpdateManager* GetUpdateManager() { return m_UpdateManager.get(); }
		const SetupFlow& GetSetupFlow() const { return m_SetupFlow; }

		const UntrustedLists::Report& GetUntrustedListReport() const { return m_UntrustedListReport; }
		/// closes the untrusted list warning; acknowledge = don't warn about the (untrusted, not blacklisted) lists again
		void DismissUntrustedListReport(bool acknowledge);

		RCONActionManager& GetActionManager() { return *m_ActionManager; }
		const RCONActionManager& GetActionManager() const { return *m_ActionManager; }

		void SetLaunchedFromSteam(bool isLaunchedFromSteam) { m_Settings.m_Unsaved.m_IsLaunchedFromSteam = isLaunchedFromSteam; }
		void SetForwardedCommandLineArguments(const std::string& str) { m_Settings.m_Unsaved.m_ForwardedCommandLineArguments = str; }

		// for easy migration
		friend class MainWindow;
	};
}
