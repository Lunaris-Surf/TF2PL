#pragma once

#include <string>
#include <string_view>

namespace tf2_bot_detector
{
	class Settings;
	class SteamID;

	/// <summary>
	/// Streamer mode (Settings > Streamer mode): while it is on, your own player is shown with a custom
	/// profile everywhere TF2PL draws players - scoreboard, dossier, chat, kill feed, overlay and the web UI.
	/// Your name, avatar and SteamID are replaced, and profile links / "Go To" sites are hidden for you.
	/// Defaults are an anonymous profile: "Anonymous", a blank avatar and a hidden SteamID.
	///
	/// Only what TF2PL *displays* changes; player lists, logs and game commands still use the real values.
	/// </summary>
	namespace StreamerMode
	{
		inline constexpr std::string_view DEFAULT_NAME = "Anonymous";
		inline constexpr std::string_view HIDDEN_STEAMID = "Hidden";

		bool IsEnabled(const Settings& settings);

		/// true if streamer mode is on and this is your own SteamID
		bool IsMasked(const Settings& settings, const SteamID& id);

		/// the custom name, or "Anonymous"
		std::string GetName(const Settings& settings);

		/// the custom SteamID text, or "Hidden"
		std::string GetSteamIDText(const Settings& settings);

		/// realName, unless id is masked (then the custom name)
		std::string DisplayName(const Settings& settings, const SteamID& id, std::string_view realName);

		/// id as SteamID3, unless masked (then the custom SteamID text)
		std::string DisplaySteamID(const Settings& settings, const SteamID& id);

		/// id as SteamID64, unless masked (then the custom SteamID text)
		std::string DisplaySteamID64(const Settings& settings, const SteamID& id);
	}
}
