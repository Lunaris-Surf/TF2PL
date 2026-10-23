#pragma once

#include <nlohmann/json_fwd.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tf2_bot_detector
{
	/// <summary>
	/// Player lists that are known to publish incorrect / fabricated cheater entries. Two tiers:
	///
	/// Untrusted (Sterling, Shitlist): unreliable data. Any copy is deleted before player lists load,
	/// auto-updates that would bring it back are refused, and other lists carrying its marker are flagged.
	///
	/// Blacklisted (Vorobey's HackerPolice, ShadeFall): run by maintainers found to be malicious
	/// (knowingly accepting fake/manipulated evidence, or an active cheater marking players with
	/// fabricated "anticheat" data). Copies are deleted, other lists carrying their marker are disabled
	/// (renamed, not deleted) instead of just flagged, and the warning cannot be permanently dismissed.
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

			// copies of a blacklisted (malicious) list that were deleted
			std::vector<std::filesystem::path> m_BlacklistedDeleted;

			// other lists carrying a blacklisted list's entries; renamed to *.blacklisted-disabled so they no longer load
			std::vector<std::filesystem::path> m_BlacklistedDisabled;

			// the user's own playerlist.json carrying blacklisted entries; never disabled, but always warned about
			std::vector<std::filesystem::path> m_BlacklistedKept;

			bool HasBlacklisted() const { return !m_BlacklistedDeleted.empty() || !m_BlacklistedDisabled.empty() || !m_BlacklistedKept.empty(); }
			bool empty() const { return m_Deleted.empty() && m_Contaminated.empty() && m_PossiblyContaminatedExports.empty() && !HasBlacklisted(); }
		};

		/// <summary>
		/// true if the text contains one of the untrusted OR blacklisted list markers (case-insensitive).
		/// </summary>
		bool ContainsUntrustedMarker(std::string_view text);

		/// <summary>
		/// true if the text contains one of the blacklisted (malicious) list markers (case-insensitive).
		/// </summary>
		bool ContainsBlacklistedMarker(std::string_view text);

		/// <summary>
		/// true if this JSON's file_info identifies it as an untrusted list.
		/// </summary>
		bool IsUntrustedListJson(const nlohmann::json& json);

		/// <summary>
		/// true if this JSON's file_info identifies it as a blacklisted (malicious) list.
		/// </summary>
		bool IsBlacklistedListJson(const nlohmann::json& json);

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
