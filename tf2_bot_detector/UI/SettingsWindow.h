#pragma once

#include "ITF2BotDetectorRenderer.h"

#include <optional>
#include <vector>

namespace tf2_bot_detector
{
	class MainWindow;
	class Settings;

	class SettingsWindow 
	{
	public:
		SettingsWindow(Settings& settings, MainWindow& mainWindow);

		void OnDraw();

	private:
		void OnDrawASOSettings();
		void OnDrawCompatibilitySettings();
		void OnDrawLoggingSettings();
		void OnDrawModerationSettings();
		void OnDrawModSettings();
		void OnDrawPerformanceSettings();
		void OnDrawGraphicsSettings();
		void OnDrawServiceIntegrationSettings();
		void OnDrawUISettings();
		void OnDrawOverlaySettings();
		void OnDrawMiscSettings();

		Settings& m_Settings;
		MainWindow& m_MainWindow;

		bool m_ModsChanged = false;

		// enumerating GPUs creates a DXGI factory; do it once, not every frame.
		std::optional<std::vector<TF2BDGpuInfo>> m_GpuList;
	};
}
