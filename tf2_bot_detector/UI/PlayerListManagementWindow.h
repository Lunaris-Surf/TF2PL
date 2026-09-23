#pragma once
#include "Config/PlayerListJSON.h"

#include <string>
#include <set>
#include <utility>
#include <vector>

namespace tf2_bot_detector
{
	class TF2BDApplication;

	class PlayerListManagementWindow
	{
	public:

		PlayerListManagementWindow();

		void Draw();
		void Open() { bOpen = true; m_NeedsRefresh = true; }
		void Refresh() { m_NeedsRefresh = true; }
		
	private:
		TF2BDApplication& m_Application;
		bool bOpen = false;
		std::string m_Search;
		std::string m_SelectedList;
		std::string m_SelectedTag;

		struct LookupRow
		{
			ConfigFileName m_FileName;
			const PlayerListData* m_Player = nullptr;
		};
		std::vector<LookupRow> m_Rows;
		std::vector<size_t> m_FilteredRows;
		std::set<std::string> m_AvailableLists;
		std::set<std::string> m_AvailableTags;
		bool m_NeedsRefresh = true;
		bool m_FilterDirty = true;
		bool m_WaitingForLists = false;
		using PlayerGenerator = decltype(std::declval<PlayerListJSON&>().GetAllPlayerData());
		using PlayerIterator = decltype(std::declval<PlayerGenerator&>().begin());
		std::optional<PlayerGenerator> m_LoadGenerator;
		std::optional<PlayerIterator> m_LoadIterator;
		size_t m_LoadedEntries = 0;
		size_t m_TotalEntries = 0;

		void RebuildCache();
		void StepCache();
		void ApplyFilters();
	};
};
