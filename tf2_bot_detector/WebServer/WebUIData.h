#pragma once

// JSON builders and the settings registry behind the web UI (see WebUI.h). Everything here runs on the app's
// update thread; WebUIController only hands the resulting strings to the HTTP threads.

#include "SteamID.h"

#include <nlohmann/json_fwd.hpp>

#include <optional>
#include <string>

namespace tf2_bot_detector
{
	class PlayerListExporter;
	class TF2BDApplication;

	namespace WebUIData
	{
		/// scoreboard, server, counters, toggles, preview/streamer state, update/untrusted/export status...
		nlohmann::json BuildState(TF2BDApplication& app, const PlayerListExporter* exporter);

		/// chat (and kill feed when enabled), oldest first
		nlohmann::json BuildChat(TF2BDApplication& app);

		/// the app log pane, oldest first
		nlohmann::json BuildLog();

		/// every setting the web UI can change, grouped into sections, with current values
		nlohmann::json BuildSettings(TF2BDApplication& app);

		/// the full dossier for one player (native: the scoreboard hover tooltip + marks by list + marked friends)
		nlohmann::json BuildDossier(TF2BDApplication& app, const SteamID& id);

		struct ListQuery
		{
			std::string m_Search;
			std::string m_List;  // empty = all lists
			std::string m_Mark;  // empty = any; attribute id or custom:tag
			bool m_OnlyWritable = false;
			size_t m_Offset = 0;
			size_t m_Limit = 200;
		};
		ListQuery ParseListQuery(const nlohmann::json& action);

		/// Player Lists browser page for a query
		nlohmann::json BuildLists(TF2BDApplication& app, const ListQuery& query);

		/// applies {id, value} from the settings panel; throws std::invalid_argument for unknown ids / bad values
		void ApplySetting(TF2BDApplication& app, const std::string& id, const nlohmann::json& value);

		/// SteamID64 / [U:1:x] / STEAM_0:x:y / profile link
		std::optional<SteamID> ParseSteamID(std::string text);
	}
}
