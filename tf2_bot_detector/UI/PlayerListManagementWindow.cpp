#include "PlayerListManagementWindow.h"

#include "Application.h"
#include "ImGui_TF2BotDetector.h"
#include "Platform/Platform.h"

#include <misc/cpp/imgui_stdlib.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <set>

namespace tf2_bot_detector
{
	namespace
	{
		std::string Lower(std::string value)
		{
			std::transform(value.begin(), value.end(), value.begin(),
				[](unsigned char c) { return char(std::tolower(c)); });
			return value;
		}

		bool HasTag(const PlayerAttributesList& attrs, const std::string& selected)
		{
			if (selected.empty())
				return true;
			for (size_t i = 0; i < size_t(PlayerAttribute::COUNT); i++)
				if (attrs.HasAttribute(PlayerAttribute(i)) && to_string(PlayerAttribute(i)) == selected)
					return true;
			return attrs.GetCustomTags().contains(selected);
		}
	}

	PlayerListManagementWindow::PlayerListManagementWindow() :
		m_Application(TF2BDApplication::GetApplication())
	{
	}

	void PlayerListManagementWindow::RebuildCache()
	{
		m_Rows.clear();
		m_FilteredRows.clear();
		m_AvailableLists.clear();
		m_AvailableTags.clear();
		m_LoadIterator.reset();
		m_LoadGenerator.reset();
		auto* playerList = m_Application.GetModLogic().GetPlayerList();
		m_TotalEntries = playerList->GetPlayerEntryCount();
		m_LoadedEntries = 0;
		m_LoadGenerator.emplace(playerList->GetAllPlayerData());
		m_LoadIterator.emplace(m_LoadGenerator->begin());
		m_NeedsRefresh = false;
		m_FilterDirty = true;
	}

	void PlayerListManagementWindow::StepCache()
	{
		if (!m_LoadIterator || !m_LoadGenerator)
			return;

		constexpr size_t ENTRIES_PER_FRAME = 2000;
		size_t processed = 0;
		while (*m_LoadIterator != m_LoadGenerator->end() && processed < ENTRIES_PER_FRAME)
		{
			const auto& [fileName, player] = **m_LoadIterator;
			m_Rows.push_back({ fileName, &player });
			if (!fileName.empty())
				m_AvailableLists.insert(fileName);
			for (const auto& tag : player.m_SavedAttributes.GetCustomTags())
				m_AvailableTags.insert(tag);
			++(*m_LoadIterator);
			processed++;
			m_LoadedEntries++;
		}

		if (*m_LoadIterator == m_LoadGenerator->end())
		{
			m_LoadIterator.reset();
			m_LoadGenerator.reset();
			ApplyFilters();
		}
	}

	void PlayerListManagementWindow::ApplyFilters()
	{
		m_FilteredRows.clear();
		m_FilteredRows.reserve(m_Rows.size());
		const auto search = Lower(m_Search);

		for (size_t i = 0; i < m_Rows.size(); i++)
		{
			const auto& row = m_Rows[i];
			const auto& player = *row.m_Player;
			if (!m_SelectedList.empty() && row.m_FileName != m_SelectedList)
				continue;
			if (!HasTag(player.m_SavedAttributes, m_SelectedTag))
				continue;

			if (!search.empty())
			{
				const std::string name = player.m_LastSeen ? player.m_LastSeen->m_PlayerName : std::string{};
				const auto searchable = Lower(name + " " + std::to_string(player.GetSteamID().GetSteamID64()) + " " +
					player.GetSteamID().GetSteamID3() + " " + player.GetSteamID().GetSteamID32());
				if (searchable.find(search) == std::string::npos)
					continue;
			}
			m_FilteredRows.push_back(i);
		}
		m_FilterDirty = false;
	}

	void PlayerListManagementWindow::Draw()
	{
		if (!bOpen || !m_Application.GetMainState())
			return;

		if (!ImGui::Begin("User Lookup", &bOpen))
		{
			ImGui::End();
			return;
		}

		if (m_NeedsRefresh)
			RebuildCache();
		StepCache();
		const bool loading = m_LoadIterator.has_value();
		if (loading)
		{
			const float fraction = m_TotalEntries > 0 ? float(m_LoadedEntries) / float(m_TotalEntries) : 0.0f;
			const auto overlay = std::to_string(m_LoadedEntries) + " / " + std::to_string(m_TotalEntries) + " entries";
			ImGui::TextUnformatted("Loading player lists...");
			ImGui::ProgressBar(fraction, { -1, 0 }, overlay.c_str());
		}
		ImGui::SetNextItemWidth(280);
		if (ImGui::InputTextWithHint("##PlayerSearch", "Name or SteamID", &m_Search))
			m_FilterDirty = true;
		ImGui::SameLine();
		if (ImGui::BeginCombo("List", m_SelectedList.empty() ? "All lists" : m_SelectedList.c_str()))
		{
			if (ImGui::Selectable("All lists", m_SelectedList.empty()))
			{
				m_SelectedList.clear();
				m_FilterDirty = true;
			}
			for (const auto& list : m_AvailableLists)
				if (ImGui::Selectable(list.c_str(), m_SelectedList == list))
				{
					m_SelectedList = list;
					m_FilterDirty = true;
				}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		if (ImGui::BeginCombo("Tag", m_SelectedTag.empty() ? "All tags" : m_SelectedTag.c_str()))
		{
			if (ImGui::Selectable("All tags", m_SelectedTag.empty()))
			{
				m_SelectedTag.clear();
				m_FilterDirty = true;
			}
			for (size_t i = 0; i < size_t(PlayerAttribute::COUNT); i++)
			{
				const auto name = to_string(PlayerAttribute(i));
				if (ImGui::Selectable(name.c_str(), m_SelectedTag == name))
				{
					m_SelectedTag = name;
					m_FilterDirty = true;
				}
			}
			for (const auto& tag : m_AvailableTags)
				if (ImGui::Selectable(tag.c_str(), m_SelectedTag == tag))
				{
					m_SelectedTag = tag;
					m_FilterDirty = true;
				}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		if (ImGui::Button("Refresh lists"))
			RebuildCache();

		if (m_FilterDirty && !loading)
			ApplyFilters();
		ImGui::TextDisabled("Showing %zu of %zu loaded entries", m_FilteredRows.size(), m_Rows.size());

		const ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
			ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
			ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
		if (ImGui::BeginTable("player_list", 7, flags, { 0, 0 }))
		{
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("Name");
			ImGui::TableSetupColumn("SteamID64");
			ImGui::TableSetupColumn("Tags");
			ImGui::TableSetupColumn("List");
			ImGui::TableSetupColumn("Proof");
			ImGui::TableSetupColumn("Last Seen");
			ImGui::TableSetupColumn("Profile");
			ImGui::TableHeadersRow();

			ImGuiListClipper clipper;
			clipper.Begin(static_cast<int>(m_FilteredRows.size()));
			while (clipper.Step())
			{
				for (int visibleRow = clipper.DisplayStart; visibleRow < clipper.DisplayEnd; visibleRow++)
				{
					const auto& row = m_Rows[m_FilteredRows[visibleRow]];
					const auto& fileName = row.m_FileName;
					const auto& player = *row.m_Player;
					const std::string name = player.m_LastSeen ? player.m_LastSeen->m_PlayerName : std::string{};

					ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(name.empty() ? "<Unknown>" : name.c_str());
				ImGui::TableNextColumn();
				ImGui::TextFmt("{}", player.GetSteamID().GetSteamID64());
				ImGui::TableNextColumn();
				ImGui::TextFmt("{}", player.m_SavedAttributes);
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(fileName.empty() ? "<Unknown source>" : fileName.c_str());
				ImGui::TableNextColumn();
				if (player.m_Proof.empty())
					ImGui::TextDisabled("None");
				else
					for (const auto& proof : player.m_Proof)
						ImGui::TextWrapped("%s", (proof.is_string() ? proof.get<std::string>() : proof.dump()).c_str());
				ImGui::TableNextColumn();
				if (player.m_LastSeen)
					ImGui::TextFmt("{}", player.m_LastSeen->m_Time);
				else
					ImGui::TextDisabled("Unknown");
				ImGui::TableNextColumn();
				ImGui::PushID(visibleRow);
				if (ImGui::SmallButton("Steam"))
					Platform::Shell::OpenURL("https://steamcommunity.com/profiles/" + std::to_string(player.GetSteamID().GetSteamID64()));
					ImGui::PopID();
				}
			}
			ImGui::EndTable();
		}
		ImGui::End();
	}
}
