#pragma once

#include "Clock.h"
#include "SteamID.h"
#include "Config/PlayerListJSON.h"

#include <chrono>
#include <optional>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

namespace tf2_bot_detector
{
	class TF2BDApplication;

	/// <summary>
	/// Player list management: browse every loaded list, look players up, add players by SteamID,
	/// and edit marks/reasons on (or remove players from) the writable local list without touching JSON.
	/// Entries from read-only lists (subscribed/official) are shown but never edited.
	/// </summary>
	class PlayerListWindow final
	{
	public:
		explicit PlayerListWindow(TF2BDApplication& app);

		void Open();
		void OpenForPlayer(const SteamID& id, std::string nameHint = {});
		bool IsOpen() const { return m_Open; }

		void Draw();

	private:
		struct Row
		{
			SteamID m_SteamID;
			ConfigFileName m_FileName;
			std::string m_LastName;
			std::optional<std::chrono::system_clock::time_point> m_LastSeen;
			PlayerAttributesList m_Attributes;
			size_t m_ProofCount = 0;
			bool m_Writable = false;
		};

		void RebuildCache();
		void ApplyFilters();
		bool RowMatches(const Row& row) const;

		void DrawToolbar();
		void DrawAddPlayer();
		void DrawTable(float height);
		void DrawDetails();
		void DrawEditor(const SteamID& id, const std::string& name);

		std::string FindBestName(const SteamID& id) const;

		TF2BDApplication& m_Application;
		bool m_Open = false;

		std::vector<Row> m_Rows;
		std::vector<size_t> m_Filtered;
		std::set<ConfigFileName> m_Lists;
		std::set<std::string> m_CustomTags;
		ConfigFileName m_WritableList;
		size_t m_CachedPlayerCount = size_t(-1);
		bool m_CachedListsLoading = false;
		bool m_NeedsRebuild = true;
		bool m_FilterDirty = true;

		// filters
		std::string m_Search;
		std::string m_ListFilter;      // empty = all lists
		int m_MarkFilter = -1;          // -1 = any; 0..COUNT-1 = PlayerAttribute; COUNT+n = custom tag n
		bool m_OnlyWritable = false;

		// selection / editing
		std::optional<SteamID> m_Selected;
		std::string m_SelectedNameHint;
		std::string m_Reason;
		std::string m_NewCustomTag;
		bool m_ConfirmRemove = false;

		// add player
		std::string m_AddSteamID;
		std::string m_AddName;
		std::string m_AddError;
	};

	/// <summary>
	/// For one player: every friend that is marked in any loaded list, with their marks and source lists.
	/// The scoreboard tooltip only shows counts by mark; this lists the actual people.
	/// </summary>
	class MarkedFriendsWindow final
	{
	public:
		explicit MarkedFriendsWindow(TF2BDApplication& app, PlayerListWindow& playerLists);

		void Open(const SteamID& id, std::string name);
		void Draw();

	private:
		struct Row
		{
			SteamID m_SteamID;
			std::string m_Name;
			bool m_InGame = false;
			PlayerMarks m_Marks;
		};

		enum class State
		{
			Loading,
			Ready,
			Private,
			Unavailable,
			PlayerGone,
		};

		void Recompute();

		TF2BDApplication& m_Application;
		PlayerListWindow& m_PlayerLists;
		bool m_Open = false;

		SteamID m_Target;
		std::string m_TargetName;

		State m_State = State::Loading;
		std::string m_Error;
		std::unordered_set<SteamID> m_FriendsSnapshot; // kept so the view survives the player leaving
		size_t m_FriendCount = 0;
		std::vector<Row> m_Rows;
		time_point_t m_LastRecompute{};
	};
}
