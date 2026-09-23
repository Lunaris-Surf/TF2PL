#include "SettingsWindow.h"
#include "MainWindow.h"
#include "ImGui_TF2BotDetector.h"
#include "Config/Settings.h"
#include "SetupFlow/AddonManagerPage.h"
#include "Util/PathUtils.h"
#include "Platform/Platform.h"
#include "Application.h"

#include <mh/algorithm/algorithm.hpp>
#include <mh/error/ensure.hpp>

#include <string>
#include <string_view>

using namespace tf2_bot_detector;

SettingsWindow::SettingsWindow(Settings& settings, MainWindow& mainWindow) :
	m_Settings(settings),
	m_MainWindow(mainWindow)
{
}

void SettingsWindow::OnDraw()
{
	OnDrawASOSettings();
	OnDrawCompatibilitySettings();
	OnDrawLoggingSettings();
	OnDrawModerationSettings();
	OnDrawModSettings();
	OnDrawPerformanceSettings();
	OnDrawServiceIntegrationSettings();
	OnDrawUISettings();
	OnDrawOverlaySettings();
	OnDrawMiscSettings();

	ImGui::NewLine();

	if (AutoLaunchTF2Checkbox(m_Settings.m_AutoLaunchTF2))
		m_Settings.SaveFile();

	if (CloseAppOnTFCloseCheckbox(m_Settings.m_ShouldCloseWhenTFCloses))
		m_Settings.SaveFile();
}

void SettingsWindow::OnDrawOverlaySettings()
{
	if (!ImGui::TreeNode("Overlay"))
		return;

	const bool tf2Running = Platform::Processes::IsTF2Running();
	const bool launchKnown = tf2Running && m_Settings.m_Unsaved.m_TF2LaunchedByUs;
	const auto HasLaunchOption = [&](const std::string_view option)
		{
			const auto padded = " " + m_Settings.m_Unsaved.m_LastLaunchedTF2Arguments + " ";
			return padded.find(" " + std::string(option) + " ") != std::string::npos;
		};
	const bool requestsWindowed = HasLaunchOption("-window") || HasLaunchOption("-sw") || HasLaunchOption("-startwindowed");
	const bool requestsFullscreen = HasLaunchOption("-fullscreen") || HasLaunchOption("-full");
	const bool verifiedExclusiveFullscreen = launchKnown && (requestsFullscreen || !requestsWindowed);

	ImGui::BeginDisabled(verifiedExclusiveFullscreen);
	if (ImGui::Checkbox("Render TF2PL over the game", &m_Settings.m_EnableGameOverlay))
		m_Settings.SaveFile();
	ImGui::EndDisabled();
	ImGui::BeginDisabled(verifiedExclusiveFullscreen);
	const int previousOverlayCorner = m_Settings.m_OverlayCorner;
	ImGui::Combo("Passive pane corner", &m_Settings.m_OverlayCorner,
		"Top left\0Top right\0Bottom left\0Bottom right\0");
	if (m_Settings.m_OverlayCorner != previousOverlayCorner)
		m_Settings.SaveFile();
	ImGui::EndDisabled();

	struct OverlayHotkeyOption
	{
		const char* m_Name;
		int m_Key;
	};
	static constexpr OverlayHotkeyOption hotkeyOptions[] = {
		{ "Tab", 0x09 }, { "Backtick / Tilde", 0xC0 },
		{ "Insert", 0x2D }, { "Home", 0x24 }, { "End", 0x23 },
		{ "Page Up", 0x21 }, { "Page Down", 0x22 },
		{ "F1", 0x70 }, { "F2", 0x71 }, { "F3", 0x72 }, { "F4", 0x73 },
		{ "F5", 0x74 }, { "F6", 0x75 }, { "F7", 0x76 }, { "F8", 0x77 },
		{ "F9", 0x78 }, { "F10", 0x79 }, { "F11", 0x7A }, { "F12", 0x7B },
		{ "A", 0x41 }, { "B", 0x42 }, { "C", 0x43 }, { "D", 0x44 },
		{ "E", 0x45 }, { "F", 0x46 }, { "G", 0x47 }, { "H", 0x48 },
		{ "I", 0x49 }, { "J", 0x4A }, { "K", 0x4B }, { "L", 0x4C },
		{ "M", 0x4D }, { "N", 0x4E }, { "O", 0x4F }, { "P", 0x50 },
		{ "Q", 0x51 }, { "R", 0x52 }, { "S", 0x53 }, { "T", 0x54 },
		{ "U", 0x55 }, { "V", 0x56 }, { "W", 0x57 }, { "X", 0x58 },
		{ "Y", 0x59 }, { "Z", 0x5A }
	};
	const char* selectedHotkeyName = "Tab";
	for (const auto& option : hotkeyOptions)
		if (option.m_Key == m_Settings.m_OverlayHotkeyKey)
			selectedHotkeyName = option.m_Name;
	bool hotkeyChanged = false;
	if (ImGui::BeginCombo("Overlay toggle key", selectedHotkeyName))
	{
		for (const auto& option : hotkeyOptions)
		{
			const bool selected = option.m_Key == m_Settings.m_OverlayHotkeyKey;
			if (ImGui::Selectable(option.m_Name, selected))
			{
				m_Settings.m_OverlayHotkeyKey = option.m_Key;
				hotkeyChanged = true;
			}
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	bool useControl = (m_Settings.m_OverlayHotkeyModifiers & 1) != 0;
	bool useAlt = (m_Settings.m_OverlayHotkeyModifiers & 2) != 0;
	bool useShift = (m_Settings.m_OverlayHotkeyModifiers & 4) != 0;
	if (ImGui::Checkbox("Ctrl", &useControl)) hotkeyChanged = true;
	ImGui::SameLine();
	if (ImGui::Checkbox("Alt", &useAlt)) hotkeyChanged = true;
	ImGui::SameLine();
	if (ImGui::Checkbox("Shift", &useShift)) hotkeyChanged = true;
	if (hotkeyChanged)
	{
		m_Settings.m_OverlayHotkeyModifiers = (useControl ? 1 : 0) |
			(useAlt ? 2 : 0) | (useShift ? 4 : 0);
		m_Settings.SaveFile();
	}

	if (verifiedExclusiveFullscreen)
	{
		if (m_Settings.m_EnableGameOverlay)
		{
			m_Settings.m_EnableGameOverlay = false;
			m_Settings.SaveFile();
		}
		ImGui::TextWrapped("Unavailable: TF2PL launched TF2 in exclusive fullscreen. Exclusive fullscreen prevents the external overlay from rendering correctly.");
	}
	else if (tf2Running && !launchKnown)
	{
		ImGui::TextColored({ 1.0f, 0.78f, 0.25f, 1.0f }, "Caution");
		ImGui::SameLine();
		ImGui::TextWrapped("TF2 was not launched by TF2PL, so its display mode cannot be verified. Enable this only when TF2 is windowed or borderless.");
	}
	else if (tf2Running)
	{
		ImGui::TextWrapped("TF2PL verified that this TF2 session was launched in a supported windowed or borderless mode.");
	}
	else
	{
		ImGui::TextWrapped("The overlay will attach when TF2 is running in windowed or borderless mode. Exclusive fullscreen is not supported.");
	}

	ImGui::TextDisabled("The overlay is an external transparent window; TF2PL does not inject anything into the game process.");
	ImGui::TextWrapped("Use the configured overlay toggle to switch interaction modes. Passive mode shows only a static marked-player pane in the selected corner. Interactive mode opens the complete TF2PL interface and blocks input from reaching TF2.");
	ImGui::NewLine();
	ImGui::TreePop();
}

void SettingsWindow::OnDrawASOSettings()
{
	if (ImGui::TreeNode("Autodetected Settings Overrides"))
	{
		// Steam dir
		if (InputTextSteamDirOverride("Steam directory", m_Settings.m_SteamDirOverride, true))
			m_Settings.SaveFile();

		// TF game dir override
		if (InputTextTFDirOverride("tf directory", m_Settings.m_TFDirOverride, FindTFDir(m_Settings.GetSteamDir()), true))
			m_Settings.SaveFile();

		// Local steamid
		if (InputTextSteamIDOverride("My Steam ID", m_Settings.m_LocalSteamIDOverride, true))
			m_Settings.SaveFile();

		ImGui::TreePop();
	}
}

void SettingsWindow::OnDrawCompatibilitySettings()
{
	if (ImGui::TreeNode("Compatibility"))
	{
		if (ImGui::Checkbox("Config Compatibility Mode", &m_Settings.m_ConfigCompatibilityMode))
			m_Settings.SaveFile();
		ImGui::SetHoverTooltip("Improves compatibility with some configs (such as mastercomfig). Resolves some strange issues with \"Issued too many commands to server\" disconnections, at the expense of delayed scoreboard updates when joining a server.");

		ImGui::NewLine();
		ImGui::TreePop();
	}
}

void SettingsWindow::OnDrawLoggingSettings()
{
	if (ImGui::TreeNode("Logging"))
	{
#ifdef TF2BD_ENABLE_DISCORD_INTEGRATION
		if (ImGui::Checkbox("Discord Rich Presence", &m_Settings.m_Logging.m_DiscordRichPresence))
			m_Settings.SaveFile();
#else
		ImGui::Checkbox("Discord Rich Presence ([!] Disabled On This Build)", &m_Settings.m_Logging.m_DiscordRichPresence);
#endif
		if (ImGui::Checkbox("RCON Packets", &m_Settings.m_Logging.m_RCONPackets))
			m_Settings.SaveFile();

		if (ImGui::Checkbox("Log Console Entries", &m_Settings.m_SaveConsoleLogs))
			m_Settings.SaveFile();

		if (ImGui::Checkbox("Log Chat History", &m_Settings.m_SaveChatHistory))
			m_Settings.SaveFile();

		ImGui::NewLine();
		ImGui::TreePop();
	}
}

void SettingsWindow::OnDrawModerationSettings()
{
	if (ImGui::TreeNode("Moderation"))
	{
		// Auto temp mute
		{
			if (ImGui::Checkbox("Auto temp mute", &m_Settings.m_AutoTempMute))
				m_Settings.SaveFile();
			ImGui::SetHoverTooltip("Automatically, temporarily mute ingame chat messages if we think someone else in the server is running the tool.");
		}

		// Auto votekick delay
		{
			if (ImGui::SliderFloat("Auto votekick delay", &m_Settings.m_AutoVotekickDelay, 0, 30, "%1.1f seconds"))
				m_Settings.SaveFile();
			ImGui::SetHoverTooltip("Delay between a player being registered as fully connected and us expecting them to be ready to vote on an issue.\n\n"
				"This is needed because players can't vote until they have joined a team and picked a class. If we call a vote before enough people are ready, it might fail.");
		}

		{
			if (ImGui::SliderInt("Minimum VoteKick Interval time", &m_Settings.m_MinVoteKickInterval, 5, 120, " % d seconds"))
				m_Settings.SaveFile();
			ImGui::SetHoverTooltip("Waits x seconds before attempting an auto Vote Kick.");
		}

		{
			if (ImGui::Checkbox("[Experimental] VoteKick Ignores Team State in certain maps", &m_Settings.m_VoteKickIgnoreTeamStateOnCertainMaps))
				m_Settings.SaveFile();
			ImGui::SetHoverTooltip("ignores team states in ze_ and vsh_");
		}

		ImGui::NewLine();

		// Send warnings for connecting cheaters
		{
			if (ImGui::Checkbox("Chat message warnings for connecting cheaters", &m_Settings.m_AutoChatWarningsConnecting))
				m_Settings.SaveFile();

			ImGui::SetHoverTooltip("Automatically sends a chat message if a cheater has joined the lobby,"
				" but is not yet in the game. Only has an effect if \"Enable Chat Warnings\""
				" is enabled (upper left of main window).\n"
				"\n"
				"Looks like: \"Heads up! There are N known cheaters joining the other team! Names unknown until they fully join.\"");
		}

		{
			if (ImGui::Checkbox("Send Chat Warnings Once Per Player", &m_Settings.m_ChatWarningSendOnce))
				m_Settings.SaveFile();
			ImGui::SetHoverTooltip("Sends chat message warnings once per cheater.");
		}

		// Chat Warning Frequency

		ImGui::BeginDisabled(m_Settings.m_ChatWarningSendOnce);
		{
			if (ImGui::SliderInt("Chat Warning Frequency", &m_Settings.m_ChatWarningInterval, 2, 60, "%d seconds"))
				m_Settings.SaveFile();
			ImGui::SetHoverTooltip("Delay between chat warnings");

			if (m_Settings.m_ChatWarningInterval < 10) {
				ImGui::TextFmt(
					{ 1, 0, 0, 1 },
					"WARN: YOU ARE SENDING MESSAGES WAY TOO FREQUENTLY! (<10s)\n"
					"People usually find this chatspamming behavior annoying, and will likely kick **you** first instead!\n"
					"(In fact, people find the default 10 second annoying, too!)\n"
					"You have been warned!"
				);
			}
		}
		ImGui::EndDisabled();

		{
			if (ImGui::Checkbox("Custom chat message warnings", &m_Settings.m_UseCustomChatWarnings))
				m_Settings.SaveFile();
			ImGui::SetHoverTooltip("Use a Custom chat warning, instead of the default \"Attention!\"...");
		}

		if (m_Settings.m_UseCustomChatWarnings)
		{
			ImGui::TextFmt(
				{ 1, 0.5f, 0, 1 },
				"NOTICE: enabling this setting will prevent other TF2PL users from recognizing your message.\n"
				"This may prevent coordination between TF2PL users and cause duplicate chat warnings.\n"
				"By using this option you understand that the above side-effect exists, and still want custom messages anyway."
			);

			ImGui::NewLine();

			ImGui::TextFmt(
				{ 1, 1, 1, 1 },
				"Tip: format your strings using {}; if you want literal '{' or '}' do {{ and }} respectively"
			);

			ImGui::InputText("Cheater Joining", &m_Settings.m_OneCheaterConnectingMessage);
			ImGui::InputText("Multiple Cheater Joining", &m_Settings.m_MultipleCheaterConnectingMessage);
			try {
				ImGui::Text(fmt::format(fmt::runtime(m_Settings.m_MultipleCheaterConnectingMessage), "3").c_str());
			}
			catch (fmt::format_error err) {
				ImGui::TextFmt({ 1, 0, 0, 1 }, "Invalid string format! this takes 1 arguments maximum! (player count)");
			}

			ImGui::NewLine();

			ImGui::InputText("Cheater Warning", &m_Settings.m_OneCheaterWarningMessage);
			try {

				ImGui::Text(fmt::format(fmt::runtime(m_Settings.m_OneCheaterWarningMessage), 1).c_str());
			}
			catch (fmt::format_error err) {
				ImGui::TextFmt( { 1, 0, 0, 1 }, "Invalid string format! this takes 1 arguments maximum! (player name)");
			}

			ImGui::InputText("Multiple Cheater Warning", &m_Settings.m_MultipleCheaterWarningMessage);
			try {
				ImGui::Text(fmt::format(fmt::runtime(m_Settings.m_MultipleCheaterWarningMessage), fmt::arg("count", 2), fmt::arg("names", "a, b")).c_str());
			}
			catch (fmt::format_error err) {
				ImGui::TextFmt({ 1, 0, 0, 1 }, "Invalid string format! this takes 2 arguments maximum! ({count}, {names})");
			}

			ImGui::NewLine();

			if (ImGui::Button("Save Custom Messages"))
				m_Settings.SaveFile();
		}

		ImGui::NewLine();

		{
			if (ImGui::Checkbox("Party message warnings for connecting cheaters", &m_Settings.m_AutoChatWarningsConnectingParty))
				m_Settings.SaveFile();

			ImGui::SetHoverTooltip("Sends a party chat message notifying a marked player is joining.");
		}

		{
			if (ImGui::Checkbox("Party Warnings: print a summary on join", &m_Settings.m_AutoChatWarningsConnectingPartyPrintSummary))
				m_Settings.SaveFile();

			ImGui::SetHoverTooltip("the [TF2PL] Currently x players are marked... message");
		}

		ImGui::NewLine();

		/*
		{
			ImGui::TextFmt("Don't warn these marks to party: (This Setting Will NOT be saved)");

			static bool selection[4] = { false, false, false, false };
			ImGui::Selectable("Cheater", &m_Settings.m_AutoChatWarningsPartyIgnore[0]);
			ImGui::Selectable("Suspicious", &m_Settings.m_AutoChatWarningsPartyIgnore[1]);
			ImGui::Selectable("Exploiter", &m_Settings.m_AutoChatWarningsPartyIgnore[2]);
			ImGui::Selectable("Racist", &m_Settings.m_AutoChatWarningsPartyIgnore[3]);
		}
		*/


		/*
		{
			if (ImGui::Checkbox("(Party) Marked VS notifications", &m_Settings.m_AutoChatWarningsMarkedVSNotifications))
				m_Settings.SaveFile();

			ImGui::SetHoverTooltip(
				"Shows VS notifications when cheaters join or leave."
			);
		}*/

		ImGui::NewLine();
		ImGui::TreePop();
	}
}

void SettingsWindow::OnDrawModSettings()
{
	if (ImGui::TreeNode("Mods"))
	{
		ImGui::NewLine();
		ImGui::TextFmt("Mods/Addons can be enabled or disabled below.");

		if (m_ModsChanged)
		{
			ImGui::NewLine();
			ImGui::TextFmt({ 1, 1, 0, 1 }, "TF2 and TF2PL must be restarted to apply changes to mods.");
		}

		IAddonManager& addonManager = IAddonManager::Get();
		for (const std::filesystem::path& path : addonManager.GetAllAvailableAddons())
		{
			const std::string filename = path.filename().string();
			ImGuiDesktop::ScopeGuards::ID id(filename);

			bool checked = addonManager.IsAddonEnabled(m_Settings, path);
			if (ImGui::Checkbox(filename.c_str(), &checked))
			{
				addonManager.SetAddonEnabled(m_Settings, path, checked);
				m_ModsChanged = true;
			}
		}

		ImGui::NewLine();
		ImGui::TreePop();
	}
}

void SettingsWindow::OnDrawPerformanceSettings()
{
	if (ImGui::TreeNode("Performance"))
	{
		// Sleep when unfocused
		{
			if (ImGui::Checkbox("Sleep when unfocused", &m_Settings.m_SleepWhenUnfocused))
				m_Settings.SaveFile();
			ImGui::SetHoverTooltip("Slows program refresh rate when not focused to reduce CPU/GPU usage.");
		}

		ImGui::NewLine();
		ImGui::TreePop();
	}
}

void SettingsWindow::OnDrawServiceIntegrationSettings()
{
	if (ImGui::TreeNode("Service Integrations"))
	{
		if (ImGui::Checkbox("Discord integrations", &m_Settings.m_Discord.m_EnableRichPresence))
			m_Settings.SaveFile();

#ifdef _DEBUG
		if (ImGui::Checkbox("Lazy Load API Data", &m_Settings.m_LazyLoadAPIData))
			m_Settings.SaveFile();
		ImGui::SetHoverTooltip("If enabled, waits until data is actually needed by the UI before requesting it, saving system resources. Otherwise, instantly loads all data from integration APIs as soon as a player joins the server.");
#endif

		if (bool allowInternet = m_Settings.m_AllowInternetUsage.value_or(false);
			ImGui::Checkbox("Allow internet connectivity", &allowInternet))
		{
			m_Settings.m_AllowInternetUsage = allowInternet;
			m_Settings.SaveFile();
		}

		ImGui::EnabledSwitch(m_Settings.m_AllowInternetUsage.value_or(false), [&](bool enabled)
			{
				const auto GetSteamAPIModeString = [](SteamAPIMode mode)
				{
					switch (mode)
					{
					case SteamAPIMode::Disabled:  return "Disabled";
					case SteamAPIMode::Direct:    return "Enabled";
					}

					LogError("Unknown value {}", mh::enum_fmt(mode));
					return "Disabled";
				};

				ImGui::NewLine();
				if (ImGui::BeginCombo("Steam API Mode", GetSteamAPIModeString(m_Settings.GetSteamAPIMode())))
				{
					const auto ModeSelectable = [&](SteamAPIMode mode)
					{
						if (ImGui::Selectable(GetSteamAPIModeString(mode), m_Settings.GetSteamAPIMode() == mode))
						{
							m_Settings.m_SteamAPIMode = mode;
							m_Settings.SaveFile();
						}
					};

					ModeSelectable(SteamAPIMode::Disabled);
					ImGui::SetHoverTooltip("Disables the Steam API integration completely.");

					ModeSelectable(SteamAPIMode::Direct);
					ImGui::SetHoverTooltip("Communicates directly with the Steam API servers.");

					ImGui::EndCombo();
				}

				if (m_Settings.GetSteamAPIMode() == SteamAPIMode::Direct)
				{
					ImGui::NewLine();
					if (std::string key = m_Settings.GetSteamAPIKey();
						InputTextSteamAPIKey("Steam API Key", key, true))
					{
						m_Settings.SetSteamAPIKey(key);
						m_Settings.SaveFile();
					}
				}

				ImGui::NewLine();

				if (ImGui::Checkbox("SteamHistory Integration", &m_Settings.m_EnableSteamHistoryIntegration))
					m_Settings.SaveFile();
				if (m_Settings.m_EnableSteamHistoryIntegration) {

					if (std::string key = m_Settings.GetSteamHistoryAPIKey();
						ImGui::InputText("SteamHistory API Key", &key))
					{
						m_Settings.SetSteamHistoryAPIKey(key);
						m_Settings.SaveFile();
					}

					if (ImGui::Button("Generate API Key"))
						tf2_bot_detector::Platform::Shell::OpenURL("https://steamhistory.net/api");
				}

				ImGui::NewLine();
				if (auto mode = enabled ? m_Settings.m_ReleaseChannel : ReleaseChannel::None;
					Combo("Automatic update checking", mode))
				{
					m_Settings.m_ReleaseChannel = mode;
					m_Settings.SaveFile();
				}
			}, "Requires \"Allow internet connectivity\"");

		ImGui::NewLine();
		ImGui::TreePop();
	}
}

void SettingsWindow::OnDrawUISettings()
{
	if (ImGui::TreeNode("UI"))
	{
		float& fontGlobalScale = ImGui::GetIO().FontGlobalScale;
		if (ImGui::Button("Reset"))
		{
			m_Settings.m_Theme.m_GlobalScale = fontGlobalScale = 1;
			m_Settings.SaveFile();
		}
		ImGui::SameLineNoPad();
		if (ImGui::SliderFloat("Global UI Scale", &fontGlobalScale, 0.5f, 2.0f,
			"%1.2f", ImGuiSliderFlags_AlwaysClamp))
		{
			m_Settings.m_Theme.m_GlobalScale = fontGlobalScale;
			m_Settings.SaveFile();
		}

		const auto GetFontComboString = [](Font f)
		{
			switch (f)
			{
			default:
				LogError("Unknown font {}", mh::enum_fmt(f));
				[[fallthrough]];
			case Font::ProggyClean_13px: return "Proggy Clean, 13px";
			case Font::ProggyClean_26px: return "Proggy Clean, 26px";
			case Font::ProggyTiny_10px:  return "Proggy Tiny, 10px";
			case Font::ProggyTiny_20px:  return "Proggy Tiny, 20px";
			case Font::UniFont_14px:  return "UniFont, 14px";
			case Font::UniFont_24px:  return "UniFont, 24px";
			}
		};

		if (ImGui::BeginCombo("Font", GetFontComboString(m_Settings.m_Theme.m_Font)))
		{
			const auto FontSelectable = [&](Font f)
			{
				ImFont* fontPtr = mh_ensure(m_MainWindow.GetFontPointer(f));
				if (!fontPtr)
					return;

				ImGui::PushFont(fontPtr);
				if (ImGui::Selectable(GetFontComboString(f), m_Settings.m_Theme.m_Font == f))
				{
					m_Settings.m_Theme.m_Font = f;
					m_Settings.SaveFile();
				}
				ImGui::PopFont();
			};

			// FIXME <- fix what please elaborate
			// what did he mean by this
			FontSelectable(Font::ProggyTiny_10px);
			FontSelectable(Font::ProggyTiny_20px);
			FontSelectable(Font::ProggyClean_13px);
			FontSelectable(Font::ProggyClean_26px);
			FontSelectable(Font::UniFont_14px);
			FontSelectable(Font::UniFont_24px);

			ImGui::EndCombo();
		}

		ImGui::NewLine();
		ImGui::TreePop();
	}
}

void tf2_bot_detector::SettingsWindow::OnDrawMiscSettings()
{
	if (ImGui::TreeNode("Misc"))
	{
		if (ImGui::Checkbox("Show kill logs on chat", &m_Settings.m_KillLogsInChat))
			m_Settings.SaveFile();

		// PASTED: see TF2CommandLinePage.cpp
		// i love tech tebt
		const auto GetTFBinaryModeString = [](TFBinaryMode mode) {
			switch (mode) {
			case TFBinaryMode::x64:  return "64-bit";
			case TFBinaryMode::x86:  return "32-bit";
			case TFBinaryMode::x86_legacy:  return "32-bit (legacy)";
			}
			return "64-bit (unknown value)";
			};

		if (ImGui::BeginCombo("TF2 Binary Mode", GetTFBinaryModeString(m_Settings.m_TFBinaryMode)))
		{
			const auto ModeSelectable = [&](TFBinaryMode mode) {
				if (ImGui::Selectable(GetTFBinaryModeString(mode), m_Settings.m_TFBinaryMode == mode)) {
					m_Settings.m_TFBinaryMode = mode;
					m_Settings.SaveFile();
				}
			};

			ModeSelectable(TFBinaryMode::x64);
			ModeSelectable(TFBinaryMode::x86);
			ModeSelectable(TFBinaryMode::x86_legacy);

			ImGui::EndCombo();
		}

		// PASTED: see TF2CommandLinePage.cpp
		if (ImGui::Checkbox("Use Static Rcon Launch Parameters (Not Recommended)", &m_Settings.m_UseRconStaticParams))
			m_Settings.SaveFile();

		if (m_Settings.m_UseRconStaticParams) {
			if (ImGui::InputText("password", &m_Settings.m_RconStaticPassword)) {
				m_Settings.SaveFile();
			}

			// imgui stuff so it's not a scalar input for ports
			std::string port = std::to_string(m_Settings.m_RconStaticPort);

			// this code has some flaws,
			// 1. it doesn't check if the port given is even a valid port
			// 2. it doesn't check if that port is available
			// however, we can just blame the user for using this option so lol
			if (ImGui::InputText("port", &port, ImGuiInputTextFlags_CharsDecimal)) {
				for (size_t i = 0; i < port.size(); ++i) {
					if (!isdigit(port.at(i))) {
						port.erase(i);
					}
				}

				m_Settings.m_RconStaticPort = std::stoi(port);
				m_Settings.SaveFile();
			}
		}

		ImGui::NewLine();
		ImGui::TreePop();
	}
}
