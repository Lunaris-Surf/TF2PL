#pragma once

#include <nlohmann/json_fwd.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tf2_bot_detector
{
	/// <summary>
	/// Player lists that are known to publish incorrect / fabricated cheater entries.
	///
	/// vorobey's "hackerpolice" list (playerlist.vorobey-hackerpolice.json) was found adding fake
	/// cheater listings. Any copy of it is deleted before player lists load, auto-updates that would
	/// bring it back are refused, and other lists carrying its marker are flagged to the user.
	/// </summary>
	namespace UntrustedLists
	{
		struct Report
		{
			// copies of an untrusted list that were deleted
			std::vector<std::filesystem::path> m_Deleted;

			// other lists that mention an untrusted list (e.g. merged/imported entries); kept but unreliable
			std::vector<std::filesystem::path> m_Contaminated;

			// combined exports that may have included an untrusted list's entries when they were made
			std::vector<std::filesystem::path> m_PossiblyContaminatedExports;

			bool empty() const { return m_Deleted.empty() && m_Contaminated.empty() && m_PossiblyContaminatedExports.empty(); }
		};

		/// <summary>
		/// true if the text contains one of the untrusted list markers (case-insensitive).
		/// </summary>
		bool ContainsUntrustedMarker(std::string_view text);

		/// <summary>
		/// true if this JSON's file_info identifies it as an untrusted list.
		/// </summary>
		bool IsUntrustedListJson(const nlohmann::json& json);

		/// <summary>
		/// Scan cfg/ for player lists: delete untrusted ones, note contaminated ones.
		/// Called before player lists are loaded. Results accumulate until TakeReport().
		/// </summary>
		void ScreenPlayerLists();

		/// <summary>
		/// What the last screens found (and clears it). Used to warn the user once per finding.
		/// </summary>
		Report TakeReport();
		bool HasReport();
	}
}
