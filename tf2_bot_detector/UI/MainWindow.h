#pragma once

#include "Actions/Actions.h"
#include "Actions/RCONActionManager.h"
#include "Clock.h"
#include "CompensatedTS.h"
#include "ConsoleLog/ConsoleLineListener.h"
#include "ConsoleLog/ConsoleLogParser.h"
#include "Config/Settings.h"
#include "Config/UntrustedLists.h"
#include "DiscordRichPresence.h"
#include "Networking/GithubAPI.h"
#include "ModeratorLogic.h"
#include "SetupFlow/SetupFlow.h"
#include "WorldEventListener.h"
#include "WorldState.h"
#include "LobbyMember.h"
#include "PlayerStatus.h"
#include "GameData/TFConstants.h"
#include "Application.h"
#include <tf2pl/error/expected.hpp>

#include <optional>
#include <vector>

struct ImFont;
struct ImVec4;

namespace tf2_bot_detector
{
	class IBaseTextures;
	class IConsoleLine;
	class IConsoleLineListener;
	class ITexture;
	class ITextureManager;
	class IUpdateManager;
	class PlayerListExporter;
	class PlayerListWindow;
	class MarkedFriendsWindow;
	class SettingsWindow;

	class MainWindow final
	{
	public:
		explicit MainWindow(TF2BDApplication* app);
		~MainWindow();

		ImFont* GetFontPointer(Font f) const;

		void DrawPlayerTooltip(IPlayer& player);
		void DrawPlayerTooltip(IPlayer& player, TeamShareResult teamShareResult, const PlayerMarks& playerAttribs);
		void DrawPlayerContextMarkMenu(const SteamID& steamid, const std::string& name, std::string& reasons);

		/// <summary>
		/// scoreboard/chat color pickers, drawn in Settings > UI > Colors.
		/// </summary>
		void DrawThemeColorSettings();

		/// Opens Player Lists (optionally focused on one player) / the marked-friends view for a player.
		void OpenPlayerListWindow();
		void OpenPlayerListWindow(const SteamID& id, const std::string& name);
		void OpenMarkedFriends(const SteamID& id, const std::string& name);

	private:
		// LunarisV chrome
		void OnDrawHeaderBar();
		void OnDrawStatusStrip();
		void OnDrawControlChips();

		void OnDrawMenuBar();
		void OnDraw();
		void OnDrawWebModeStatus(); // replaces the main panes while Interface = Web
		void OnDrawPreviewBanner();  // "you're looking at a sample lobby" strip + exit button
		void OnDrawPreviewOffer();   // "explore TF2PL without TF2" on the launch/setup panel
		void OnEndFrame();
		void OnDrawScoreboard();
		void OnDrawTeamStats();
		void OnDrawAllPanesDisabled();

		void OnDrawStats();
		void OnDrawScoreboardAndLog(float width, float height);
		void OnDrawExportWindow();
		void DrawPassiveOverlayPane();

		void OnDrawScoreboardContextMenu(IPlayer& player);
		void OnDrawScoreboardRow(IPlayer& player);
		void OnDrawColorPicker(const char* name_id, std::array<float, 4>& color);
		void OnDrawChat();
		void OnDrawServerStats();
		void DrawPlayerTooltipBody(IPlayer& player, TeamShareResult teamShareResult, const PlayerMarks& playerAttribs);

		struct ColorPicker
		{
			const char* m_Name;
			std::array<float, 4>& m_Color;
		};
		void OnDrawColorPickers(const char* id, const std::initializer_list<ColorPicker>& pickers);

		void OnDrawAppLog();

		bool b_SettingsOpen = false;
		void OnDrawSettings();
		void ToggleSettingsPopup();

		void OnDrawUpdateCheckPopup();
		bool m_UpdateCheckPopupOpen = false;
		void OpenUpdateCheckPopup();

		void OnDrawAboutPopup();
		bool m_AboutPopupOpen = false;
		void OpenAboutPopup() { m_AboutPopupOpen = true; }

		void PrintDebugInfo();

		bool IsSleepingEnabled() const;

		void SetupFonts();

		// these fonts do not support unicode.
		ImFont* m_ProggyTiny10Font{};
		ImFont* m_ProggyTiny20Font{};
		/* m_ProggyClean13Font managed by imgui */
		ImFont* m_ProggyClean26Font{};

		// this font supports unicode.
		ImFont* m_Unifont14Font{};
		ImFont* m_Unifont24Font{};

		// LunarisV body font (Chakra Petch with unifont merged in for non-latin names)
		ImFont* m_ChakraPetch16Font{};


		TF2BDApplication* m_Application;

		tf2pl::expected<std::shared_ptr<ITexture>, std::error_condition> TryGetAvatarTexture(IPlayer& player);

		// streamer mode's custom avatar (Settings > Streamer mode), reloaded when the path changes
		tf2pl::expected<std::shared_ptr<ITexture>, std::error_condition> GetStreamerAvatarTexture();
		std::string m_StreamerAvatarPath;
		std::shared_ptr<ITexture> m_StreamerAvatar;
		bool m_StreamerAvatarFailed = false;
		std::shared_ptr<ITextureManager> m_TextureManager;
		std::unique_ptr<IBaseTextures> m_BaseTextures;

		Settings& m_Settings;
		std::unique_ptr<SettingsWindow> m_SettingsWindow;
		std::unique_ptr<PlayerListWindow> m_PlayerListWindow;
		std::unique_ptr<MarkedFriendsWindow> m_MarkedFriendsWindow;

		// warning when an untrusted player list was removed / other lists carry its entries
		// (the pending report lives in TF2BDApplication so the web UI can show and dismiss it too)
		void OnDrawUntrustedListWarning();

		// first-run "which GPU?" prompt
		void OnDrawGpuPrompt();
		std::optional<std::vector<TF2BDGpuInfo>> m_GpuPromptList;
		bool m_GpuRestartNotice = false;
		std::unique_ptr<PlayerListExporter> m_PlayerExporter;
		bool m_ExportWindowOpen = false;

		/// <summary>
		/// for "sleep when unfocused" feature.
		/// note: it still will update, when a new log as been processed.
		/// </summary>
		bool b_ShouldUpdate = false;

	public:
		void QueueUpdate();
		bool ShouldUpdate();

		/// <summary>
		/// Draw our imgui menu
		///
		/// IMPORTANT NOTE: this also handles b_ShouldUpdate/QueueUpdate
		///  see: MainWindow::OnDrawAppLog
		/// </summary>
		void Draw();

		// void DrawExternal();

		void OnUpdate();
		void OnImGuiInit();
		void OpenGLInit();
	};
}
