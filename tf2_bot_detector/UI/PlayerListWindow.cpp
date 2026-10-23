#include "PlayerListWindow.h"
#include "Application.h"
#include "Log.h"
#include "ModeratorLogic.h"
#include "WorldState.h"
#include "Config/Settings.h"
#include "GameData/IPlayer.h"
#include "Networking/HTTPHelpers.h"
#include "Networking/SteamAPI.h"
#include "UI/AppStyle.h"
#include "UI/ImGui_TF2BotDetector.h"
#include "UI/LunarisUI.h"
#include "PreviewMode.h"
#include "StreamerMode.h"

#include <fmt/chrono.h>
#include <fmt/format.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <tf2pl/text/case_insensitive_string.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace tf2_bot_detector;

namespace
{
	bool ContainsNoCase(std::string_view haystack, std::string_view needle)
	{
		if (needle.empty())
			return true;

		return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
			[](char a, char b) { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); }) != haystack.end();
	}

	std::string DescribeAttributes(const PlayerAttributesList& attrs)
	{
		std::string out;
		for (size_t i = 0; i < PlayerAttributesList::size(); i++)
		{
			const auto attr = PlayerAttribute(i);
			if (!attrs.HasAttribute(attr))
				continue;
			if (!out.empty())
				out += ", ";
			out += to_string(attr);
		}
		for (const auto& tag : attrs.GetCustomTags())
		{
			if (!out.empty())
				out += ", ";
			out += tag.starts_with("custom:") ? tag.substr(7) : tag;
		}
		return out;
	}

	std::string DisplayListName(const ConfigFileName& name, const ConfigFileName& writable)
	{
		if (name == writable)
			return name.empty() ? "playerlist.json (your list)"s : fmt::format("{} (your list)", name);
		return name.empty() ? "(unnamed list)"s : name;
	}

	/// Accepts SteamID64 (7656...), SteamID3 ([U:1:123]) and SteamID2 (STEAM_0:1:23).
	std::optional<SteamID> ParseSteamID(std::string text)
	{
		std::erase_if(text, [](char c) { return std::isspace((unsigned char)c); });
		if (text.empty())
			return std::nullopt;

		// profile links: .../profiles/7656xxxxxxxxxxxxx[/]
		if (auto pos = text.find("/profiles/"); pos != text.npos)
		{
			text = text.substr(pos + 10);
			if (auto slash = text.find('/'); slash != text.npos)
				text.resize(slash);
		}

		try
		{
			SteamID id(text);
			if (id.IsValid())
				return id;
		}
		catch (...)
		{
		}

		return std::nullopt;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// PlayerListWindow

PlayerListWindow::PlayerListWindow(TF2BDApplication& app) :
	m_Application(app)
{
}

void PlayerListWindow::Open()
{
	m_Open = true;
	m_NeedsRebuild = true;
}

void PlayerListWindow::OpenForPlayer(const SteamID& id, std::string nameHint)
{
	Open();
	m_Selected = id;
	m_SelectedNameHint = std::move(nameHint);
	m_ConfirmRemove = false;
}

void PlayerListWindow::RebuildCache()
{
	m_NeedsRebuild = false;
	m_FilterDirty = true;
	m_Rows.clear();
	m_Lists.clear();

	auto* playerList = m_Application.GetModLogic().GetPlayerList();
	if (!playerList)
		return;

	m_WritableList = playerList->GetWritableListName();
	m_CustomTags = playerList->GetCustomTags();
	m_CachedPlayerCount = playerList->GetPlayerCount();
	m_CachedListsLoading = playerList->ArePlayerListsLoading();

	m_Rows.reserve(m_CachedPlayerCount);
	for (const auto& [fileName, data] : playerList->GetAllPlayerData())
	{
		const auto attrs = data.GetAttributes();
		if (attrs.empty())
			continue; // placeholder entries (no marks) are not saved and not worth listing

		Row& row = m_Rows.emplace_back();
		row.m_SteamID = data.GetSteamID();
		row.m_FileName = fileName;
		row.m_Attributes = attrs;
		row.m_ProofCount = data.m_Proof.size();
		row.m_Writable = (fileName == m_WritableList);
		if (data.m_LastSeen)
		{
			row.m_LastName = data.m_LastSeen->m_PlayerName;
			row.m_LastSeen = data.m_LastSeen->m_Time;
		}

		m_Lists.insert(fileName);
	}
}

bool PlayerListWindow::RowMatches(const Row& row) const
{
	if (m_OnlyWritable && !row.m_Writable)
		return false;
	if (!m_ListFilter.empty() && row.m_FileName != m_ListFilter)
		return false;

	if (m_MarkFilter >= 0)
	{
		if (m_MarkFilter < int(PlayerAttribute::COUNT))
		{
			if (!row.m_Attributes.HasAttribute(PlayerAttribute(m_MarkFilter)))
				return false;
		}
		else
		{
			const size_t tagIndex = size_t(m_MarkFilter) - size_t(PlayerAttribute::COUNT);
			if (tagIndex >= m_CustomTags.size())
				return false;
			const auto& tag = *std::next(m_CustomTags.begin(), tagIndex);
			if (!row.m_Attributes.GetCustomTags().contains(tag))
				return false;
		}
	}

	if (!m_Search.empty())
	{
		if (!ContainsNoCase(row.m_LastName, m_Search) &&
			!ContainsNoCase(row.m_SteamID.str(), m_Search) &&
			!ContainsNoCase(std::to_string(row.m_SteamID.GetSteamID64()), m_Search))
		{
			return false;
		}
	}

	return true;
}

void PlayerListWindow::ApplyFilters()
{
	m_FilterDirty = false;
	m_Filtered.clear();
	for (size_t i = 0; i < m_Rows.size(); i++)
	{
		if (RowMatches(m_Rows[i]))
			m_Filtered.push_back(i);
	}
}

std::string PlayerListWindow::FindBestName(const SteamID& id) const
{
	if (StreamerMode::IsMasked(m_Application.GetSettings(), id))
		return StreamerMode::GetName(m_Application.GetSettings());

	if (auto player = m_Application.GetWorld().FindPlayer(id))
		return player->GetNameSafe();

	std::string best;
	std::optional<std::chrono::system_clock::time_point> bestTime;
	if (auto* playerList = m_Application.GetModLogic().GetPlayerList())
	{
		for (const auto& [fileName, data] : playerList->FindPlayerData(id))
		{
			if (data.m_LastSeen && !data.m_LastSeen->m_PlayerName.empty() &&
				(!bestTime || data.m_LastSeen->m_Time > *bestTime))
			{
				best = data.m_LastSeen->m_PlayerName;
				bestTime = data.m_LastSeen->m_Time;
			}
		}
	}

	if (best.empty() && m_Selected && *m_Selected == id)
		best = m_SelectedNameHint;

	return best;
}

void PlayerListWindow::Draw()
{
	if (!m_Open || !m_Application.GetMainState())
		return;

	auto* playerList = m_Application.GetModLogic().GetPlayerList();
	if (playerList)
	{
		// lists finish loading/updating in the background; pick up the new data when they do
		if (playerList->GetPlayerCount() != m_CachedPlayerCount ||
			playerList->ArePlayerListsLoading() != m_CachedListsLoading)
		{
			m_NeedsRebuild = true;
		}
	}

	if (m_NeedsRebuild)
		RebuildCache();
	if (m_FilterDirty)
		ApplyFilters();

	ImGui::SetNextWindowSize({ 980, 620 }, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints({ 640, 360 }, { FLT_MAX, FLT_MAX });
	const bool visible = LunarisUI::BeginWindow("Player Lists##PlayerListWindow", "PLAYER LISTS", &m_Open);
	if (visible)
	{
		DrawToolbar();
		LunarisUI::Rule();

		const float detailsWidth = 360.0f;
		const float tableWidth = std::max(240.0f, ImGui::GetContentRegionAvail().x - detailsWidth - ImGui::GetStyle().ItemSpacing.x);

		ImGui::BeginChild("##table", { tableWidth, 0 });
		DrawTable(ImGui::GetContentRegionAvail().y);
		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("##details", { 0, 0 }, ImGuiChildFlags_Border);
		DrawDetails();
		ImGui::EndChild();
	}
	ImGui::End();
}

void PlayerListWindow::DrawToolbar()
{
	using namespace LunarisPalette;

	ImGui::SetNextItemWidth(240);
	if (ImGui::InputTextWithHint("##search", "Search name or SteamID", &m_Search))
		m_FilterDirty = true;

	ImGui::SameLine();
	ImGui::SetNextItemWidth(230);
	const std::string listPreview = m_ListFilter.empty() ? "All lists"s : DisplayListName(m_ListFilter, m_WritableList);
	if (ImGui::BeginCombo("##list", listPreview.c_str()))
	{
		if (ImGui::Selectable("All lists", m_ListFilter.empty()))
		{
			m_ListFilter.clear();
			m_FilterDirty = true;
		}
		for (const auto& list : m_Lists)
		{
			if (ImGui::Selectable(DisplayListName(list, m_WritableList).c_str(), m_ListFilter == list))
			{
				m_ListFilter = list;
				m_FilterDirty = true;
			}
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	ImGui::SetNextItemWidth(170);
	std::string markPreview = "Any mark";
	if (m_MarkFilter >= 0 && m_MarkFilter < int(PlayerAttribute::COUNT))
		markPreview = to_string(PlayerAttribute(m_MarkFilter));
	else if (m_MarkFilter >= int(PlayerAttribute::COUNT))
	{
		const size_t tagIndex = size_t(m_MarkFilter) - size_t(PlayerAttribute::COUNT);
		if (tagIndex < m_CustomTags.size())
			markPreview = std::next(m_CustomTags.begin(), tagIndex)->substr(7);
	}
	if (ImGui::BeginCombo("##mark", markPreview.c_str()))
	{
		if (ImGui::Selectable("Any mark", m_MarkFilter < 0))
		{
			m_MarkFilter = -1;
			m_FilterDirty = true;
		}
		for (int i = 0; i < int(PlayerAttribute::COUNT); i++)
		{
			if (ImGui::Selectable(to_string(PlayerAttribute(i)).c_str(), m_MarkFilter == i))
			{
				m_MarkFilter = i;
				m_FilterDirty = true;
			}
		}
		int tagIndex = 0;
		for (const auto& tag : m_CustomTags)
		{
			const int value = int(PlayerAttribute::COUNT) + tagIndex++;
			if (ImGui::Selectable(tag.substr(7).c_str(), m_MarkFilter == value))
			{
				m_MarkFilter = value;
				m_FilterDirty = true;
			}
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	if (ImGui::Checkbox("Only my list", &m_OnlyWritable))
		m_FilterDirty = true;

	ImGui::SameLine();
	if (LunarisUI::SkewButton("Refresh##PlayerLists"))
		m_NeedsRebuild = true;

	ImGui::SameLine();
	if (LunarisUI::SkewButton("Add player##PlayerLists", LunarisUI::ButtonStyle::Primary))
	{
		m_AddError.clear();
		ImGui::OpenPopup("Add player##PlayerListsAdd");
	}
	DrawAddPlayer();

	ImGui::TextColored(Muted, "%zu of %zu entries across %zu list(s)%s", m_Filtered.size(), m_Rows.size(), m_Lists.size(),
		m_CachedListsLoading ? " - some lists are still loading" : "");
}

void PlayerListWindow::DrawAddPlayer()
{
	using namespace LunarisPalette;

	ImGui::SetNextWindowSize({ 420, 0 });
	if (!ImGui::BeginPopup("Add player##PlayerListsAdd"))
		return;

	LunarisUI::MicroLabel("Add or look up a player");
	ImGui::TextColored(Muted, "SteamID64, [U:1:...], STEAM_0:... or a steamcommunity.com/profiles/ link");

	ImGui::SetNextItemWidth(-1);
	if (ImGui::IsWindowAppearing())
		ImGui::SetKeyboardFocusHere();
	const bool enter = ImGui::InputTextWithHint("##addsteamid", "SteamID", &m_AddSteamID, ImGuiInputTextFlags_EnterReturnsTrue);
	ImGui::SetNextItemWidth(-1);
	ImGui::InputTextWithHint("##addname", "Name (optional, remembered as last seen name)", &m_AddName);

	if (!m_AddError.empty())
		ImGui::TextColored(Red, "%s", m_AddError.c_str());

	if (LunarisUI::SkewButton("Continue##AddPlayer", LunarisUI::ButtonStyle::Primary) || enter)
	{
		if (auto id = ParseSteamID(m_AddSteamID))
		{
			if (*id == m_Application.GetSettings().GetLocalSteamID())
			{
				m_AddError = "That's your own SteamID - you can't mark yourself.";
			}
			else
			{
				OpenForPlayer(*id, m_AddName);
				m_AddSteamID.clear();
				m_AddName.clear();
				m_AddError.clear();
				ImGui::CloseCurrentPopup();
			}
		}
		else
		{
			m_AddError = "Not a valid SteamID.";
		}
	}
	ImGui::SameLine();
	if (LunarisUI::SkewButton("Cancel##AddPlayer", LunarisUI::ButtonStyle::Ghost))
		ImGui::CloseCurrentPopup();

	ImGui::TextColored(Muted, "Pick marks for the player on the right; nothing is saved until you tick one.");
	ImGui::EndPopup();
}

void PlayerListWindow::DrawTable(float height)
{
	using namespace LunarisPalette;

	constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;

	if (!ImGui::BeginTable("##players", 4, flags, { 0, height }))
		return;

	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.2f);
	ImGui::TableSetupColumn("SteamID", ImGuiTableColumnFlags_WidthStretch, 1.0f);
	ImGui::TableSetupColumn("Marks", ImGuiTableColumnFlags_WidthStretch, 1.4f);
	ImGui::TableSetupColumn("List", ImGuiTableColumnFlags_WidthStretch, 1.0f);
	ImGui::TableHeadersRow();

	ImGuiListClipper clipper;
	clipper.Begin(int(m_Filtered.size()));
	while (clipper.Step())
	{
		for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
		{
			const Row& row = m_Rows[m_Filtered[size_t(i)]];
			ImGui::PushID(i);
			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			const bool selected = m_Selected && *m_Selected == row.m_SteamID;
			const bool masked = StreamerMode::IsMasked(m_Application.GetSettings(), row.m_SteamID);
			const std::string label = masked ? StreamerMode::GetName(m_Application.GetSettings()) :
				(row.m_LastName.empty() ? "<unknown name>"s : row.m_LastName);
			if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
			{
				m_Selected = row.m_SteamID;
				m_SelectedNameHint = row.m_LastName;
				m_ConfirmRemove = false;
			}

			ImGui::TableNextColumn();
			if (LunarisUI::Fonts().m_Mono)
				ImGui::PushFont(LunarisUI::Fonts().m_Mono);
			ImGui::TextUnformatted(StreamerMode::DisplaySteamID(m_Application.GetSettings(), row.m_SteamID).c_str());
			if (LunarisUI::Fonts().m_Mono)
				ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextColored(Orange, "%s", DescribeAttributes(row.m_Attributes).c_str());

			ImGui::TableNextColumn();
			ImGui::TextColored(row.m_Writable ? Teal : Muted, "%s", DisplayListName(row.m_FileName, m_WritableList).c_str());

			ImGui::PopID();
		}
	}

	ImGui::EndTable();
}

void PlayerListWindow::DrawDetails()
{
	using namespace LunarisPalette;

	if (!m_Selected)
	{
		ImGui::TextColored(Muted, "Select a player, or use Add player to look someone up by SteamID.");
		return;
	}

	const SteamID id = *m_Selected;
	const std::string name = FindBestName(id);

	LunarisUI::MicroLabel("Player");
	ImGui::TextUnformatted(name.empty() ? "<unknown name>" : name.c_str());
	if (LunarisUI::Fonts().m_Mono)
		ImGui::PushFont(LunarisUI::Fonts().m_Mono);
	const bool masked = StreamerMode::IsMasked(m_Application.GetSettings(), id);
	if (masked)
		ImGui::TextColored(Muted, "%s", StreamerMode::GetSteamIDText(m_Application.GetSettings()).c_str());
	else
		ImGui::TextColored(Muted, "%s   %llu", id.str().c_str(), (unsigned long long)id.GetSteamID64());
	if (LunarisUI::Fonts().m_Mono)
		ImGui::PopFont();

	ImGui::BeginDisabled(masked);
	if (LunarisUI::SkewButton("Copy SteamID64##Details", LunarisUI::ButtonStyle::Ghost))
		ImGui::SetClipboardText(std::to_string(id.GetSteamID64()).c_str());
	ImGui::EndDisabled();
	ImGui::SameLine();
	DrawPlayerContextGoToMenu(m_Application.GetSettings(), id);

	if (m_Application.GetWorld().FindPlayer(id))
		ImGui::TextColored(Green, "In your current game");

	auto* preview = m_Application.GetPreview();
	const bool samplePlayer = preview && preview->OwnsPlayer(id);
	if (samplePlayer)
		ImGui::TextColored(Orange, "Preview mode sample player: marks here are kept in memory and never saved.");

	ImGui::Spacing();
	LunarisUI::MicroLabel("Marks by list");

	auto* playerList = m_Application.GetModLogic().GetPlayerList();
	bool anyMarks = false;
	if (samplePlayer)
	{
		for (const auto& mark : m_Application.GetModLogic().GetPlayerAttributes(id))
		{
			anyMarks = true;
			ImGui::TextColored(Muted, "%s", mark.m_FileName.c_str());
			ImGui::Indent();
			ImGui::TextColored(Orange, "%s", DescribeAttributes(mark.m_Attributes).c_str());
			ImGui::Unindent();
		}
	}
	else if (playerList)
	{
		for (const auto& [fileName, data] : playerList->FindPlayerData(id))
		{
			const auto attrs = data.GetAttributes();
			if (attrs.empty())
				continue;

			anyMarks = true;
			ImGui::TextColored(fileName == m_WritableList ? Teal : Muted, "%s", DisplayListName(fileName, m_WritableList).c_str());
			ImGui::Indent();
			ImGui::TextColored(Orange, "%s", DescribeAttributes(attrs).c_str());
			for (const auto& proof : data.m_Proof)
			{
				const std::string text = proof.is_string() ? proof.get<std::string>() : proof.dump();
				ImGui::PushTextWrapPos(0);
				ImGui::TextColored(Muted, "- %s", text.c_str());
				ImGui::PopTextWrapPos();
			}
			if (data.m_LastSeen)
			{
				ImGui::TextColored(Dim, "last seen %s%s",
					fmt::format("{:%Y-%m-%d}", std::chrono::floor<std::chrono::seconds>(data.m_LastSeen->m_Time)).c_str(),
					data.m_LastSeen->m_PlayerName.empty() ? "" : fmt::format(" as {}", data.m_LastSeen->m_PlayerName).c_str());
			}
			ImGui::Unindent();
		}
	}
	if (!anyMarks)
		ImGui::TextColored(Muted, "Not marked in any loaded list.");

	ImGui::Spacing();
	LunarisUI::Rule();
	DrawEditor(id, name);
}

void PlayerListWindow::DrawEditor(const SteamID& id, const std::string& name)
{
	using namespace LunarisPalette;

	LunarisUI::MicroLabel("Your list");

	if (id == m_Application.GetSettings().GetLocalSteamID())
	{
		ImGui::TextColored(Muted, "This is you - your own SteamID can't be marked.");
		return;
	}

	auto& modLogic = m_Application.GetModLogic();
	auto* playerList = modLogic.GetPlayerList();
	if (!playerList)
		return;

	ImGui::SetNextItemWidth(-1);
	ImGui::InputTextWithHint("##reason", "Reason / proof (saved with the next change)", &m_Reason);

	const auto editable = m_Application.GetEditablePlayerAttributes(id); // in-memory for preview sample players
	bool changed = false;

	if (ImGui::BeginTable("##editmarks", 2))
	{
		for (int i = 0; i < int(PlayerAttribute::COUNT); i++)
		{
			const auto attr = PlayerAttribute(i);
			bool value = editable.HasAttribute(attr);
			ImGui::TableNextColumn();
			if (ImGui::Checkbox(to_string(attr).c_str(), &value))
			{
				if (modLogic.SetPlayerAttribute(id, name, attr, AttributePersistence::Saved, value, m_Reason))
				{
					Log("Player lists: {} {} {:v} | {}", name.empty() ? id.str() : name, value ? "marked" : "unmarked",
						tf2pl::enum_fmt(attr), m_Reason);
					changed = true;
				}
			}
		}
		ImGui::EndTable();
	}

	if (!m_CustomTags.empty() || !editable.GetCustomTags().empty())
	{
		ImGui::TextColored(Muted, "Custom tags");
		std::set<std::string> tags = m_CustomTags;
		tags.insert(editable.GetCustomTags().begin(), editable.GetCustomTags().end());
		for (const auto& tag : tags)
		{
			bool value = editable.GetCustomTags().contains(tag);
			if (ImGui::Checkbox(tag.substr(7).c_str(), &value))
				changed |= modLogic.SetPlayerCustomTag(id, name, tag, AttributePersistence::Saved, value, m_Reason);
		}
	}

	ImGui::SetNextItemWidth(200);
	ImGui::InputTextWithHint("##newtag", "new tag, e.g. watchlist", &m_NewCustomTag);
	const std::string newTag = "custom:" + m_NewCustomTag;
	const bool validTag = PlayerAttributesList::IsValidCustomTag(newTag);
	ImGui::SameLine();
	ImGui::BeginDisabled(!validTag);
	if (LunarisUI::SkewButton("Add tag##Editor", LunarisUI::ButtonStyle::Secondary))
	{
		changed |= modLogic.SetPlayerCustomTag(id, name, newTag, AttributePersistence::Saved, true, m_Reason);
		m_NewCustomTag.clear();
	}
	ImGui::EndDisabled();
	if (!m_NewCustomTag.empty() && !validTag)
		ImGui::TextColored(Red, "1-64 lowercase letters, digits, - or _; start with a letter.");

	ImGui::Spacing();
	auto* preview = m_Application.GetPreview();
	const bool inWritableList = !editable.empty() && !(preview && preview->OwnsPlayer(id));
	ImGui::BeginDisabled(!inWritableList);
	if (!m_ConfirmRemove)
	{
		if (LunarisUI::SkewButton("Remove from my list##Editor", LunarisUI::ButtonStyle::Ghost))
			m_ConfirmRemove = true;
	}
	else
	{
		ImGui::TextColored(Orange, "Remove all of your marks and reasons for this player?");
		if (LunarisUI::SkewButton("Yes, remove##Editor", LunarisUI::ButtonStyle::Primary))
		{
			if (playerList->RemoveFromWritableList(id) == ModifyPlayerResult::FileSaved)
			{
				Log("Player lists: removed {} from {}", name.empty() ? id.str() : name, m_WritableList);
				changed = true;
			}
			m_ConfirmRemove = false;
		}
		ImGui::SameLine();
		if (LunarisUI::SkewButton("Cancel##RemoveEditor", LunarisUI::ButtonStyle::Ghost))
			m_ConfirmRemove = false;
	}
	ImGui::EndDisabled();

	ImGui::TextColored(Dim, "Only your own list is changed. Marks from subscribed lists stay until that list changes.");

	if (changed)
	{
		m_Reason.clear();
		m_NeedsRebuild = true;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// MarkedFriendsWindow

MarkedFriendsWindow::MarkedFriendsWindow(TF2BDApplication& app, PlayerListWindow& playerLists) :
	m_Application(app), m_PlayerLists(playerLists)
{
}

void MarkedFriendsWindow::Open(const SteamID& id, std::string name)
{
	m_Open = true;
	m_Target = id;
	m_TargetName = StreamerMode::DisplayName(m_Application.GetSettings(), id, name);
	m_FriendsSnapshot.clear();
	m_FriendCount = 0;
	m_Rows.clear();
	m_State = State::Loading;
	Recompute();
}

void MarkedFriendsWindow::Recompute()
{
	m_LastRecompute = tfbd_clock_t::now();
	const auto& world = m_Application.GetWorld();
	auto& modLogic = m_Application.GetModLogic();

	if (const IPlayer* player = world.FindPlayer(m_Target))
	{
		const auto& friends = player->GetFriendsInfo();
		if (friends.has_value())
		{
			m_FriendsSnapshot = friends.value().m_Friends;
			m_State = State::Ready;
		}
		else if (m_FriendsSnapshot.empty())
		{
			const auto err = friends.error();
			if (err == std::errc::operation_in_progress)
				m_State = State::Loading;
			else if (err == HTTPResponseCode::Unauthorized || err == SteamAPI::ErrorCode::InfoPrivate)
				m_State = State::Private;
			else
			{
				m_State = State::Unavailable;
				m_Error = err.message();
			}
		}
	}
	else if (m_FriendsSnapshot.empty())
	{
		m_State = State::PlayerGone;
	}

	m_FriendCount = m_FriendsSnapshot.size();
	m_Rows.clear();
	for (const SteamID& friendID : m_FriendsSnapshot)
	{
		auto marks = modLogic.GetPlayerAttributes(friendID);
		if (marks.empty())
			continue;

		Row& row = m_Rows.emplace_back();
		row.m_SteamID = friendID;
		row.m_Marks = std::move(marks);
		if (const IPlayer* inGame = world.FindPlayer(friendID))
		{
			row.m_InGame = true;
			row.m_Name = StreamerMode::DisplayName(m_Application.GetSettings(), friendID, inGame->GetNameSafe());
		}
		else if (auto* playerList = modLogic.GetPlayerList())
		{
			for (const auto& [fileName, data] : playerList->FindPlayerData(friendID))
			{
				if (data.m_LastSeen && !data.m_LastSeen->m_PlayerName.empty())
				{
					row.m_Name = data.m_LastSeen->m_PlayerName;
					break;
				}
			}
		}
	}

	// in-game friends first, then by name
	std::sort(m_Rows.begin(), m_Rows.end(), [](const Row& a, const Row& b)
		{
			if (a.m_InGame != b.m_InGame)
				return a.m_InGame;
			return a.m_Name < b.m_Name;
		});
}

void MarkedFriendsWindow::Draw()
{
	using namespace LunarisPalette;

	if (!m_Open || !m_Application.GetMainState())
		return;

	// friends lists arrive asynchronously; keep checking until we have one (cheap, at most twice a second)
	if (m_State == State::Loading && tfbd_clock_t::now() - m_LastRecompute > 500ms)
		Recompute();

	ImGui::SetNextWindowSize({ 720, 420 }, ImGuiCond_FirstUseEver);
	const bool visible = LunarisUI::BeginWindow("Marked Friends##MarkedFriendsWindow", "MARKED FRIENDS", &m_Open);
	if (visible)
	{
		const std::string targetID = StreamerMode::DisplaySteamID(m_Application.GetSettings(), m_Target);
		ImGui::TextUnformatted(m_TargetName.empty() ? targetID.c_str() : m_TargetName.c_str());
		ImGui::SameLine();
		ImGui::TextColored(Muted, "%s", targetID.c_str());
		ImGui::SameLine();
		if (LunarisUI::SkewButton("Refresh##MarkedFriends"))
			Recompute();

		switch (m_State)
		{
		case State::Loading:
			ImGui::TextColored(Muted, "Waiting for this player's friends list from Steam...");
			break;
		case State::Private:
			ImGui::TextColored(Muted, "This player's friends list is private, so marked friends can't be checked.");
			break;
		case State::Unavailable:
			ImGui::TextColored(Red, "Friends list unavailable: %s", m_Error.c_str());
			break;
		case State::PlayerGone:
			ImGui::TextColored(Muted, "This player is no longer in your game and their friends list wasn't loaded.");
			break;
		case State::Ready:
			ImGui::TextColored(m_Rows.empty() ? Muted : Orange, "%zu of %zu friends are marked", m_Rows.size(), m_FriendCount);
			break;
		}

		LunarisUI::Rule();

		constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;
		if (!m_Rows.empty() && ImGui::BeginTable("##markedfriends", 4, flags))
		{
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.2f);
			ImGui::TableSetupColumn("SteamID", ImGuiTableColumnFlags_WidthStretch, 1.0f);
			ImGui::TableSetupColumn("Marks", ImGuiTableColumnFlags_WidthStretch, 1.4f);
			ImGui::TableSetupColumn("Source list(s)", ImGuiTableColumnFlags_WidthStretch, 1.2f);
			ImGui::TableHeadersRow();

			for (size_t i = 0; i < m_Rows.size(); i++)
			{
				const Row& row = m_Rows[i];
				ImGui::PushID(int(i));
				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				const std::string label = row.m_Name.empty() ? "<unknown name>"s : row.m_Name;
				if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick) &&
					ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					m_PlayerLists.OpenForPlayer(row.m_SteamID, row.m_Name);
				}
				ImGui::SetHoverTooltip("Double-click to open in Player Lists");
				if (row.m_InGame)
				{
					ImGui::SameLine();
					ImGui::TextColored(Green, "in game");
				}

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(StreamerMode::DisplaySteamID(m_Application.GetSettings(), row.m_SteamID).c_str());

				PlayerAttributesList combined;
				std::string sources;
				for (const auto& mark : row.m_Marks)
				{
					combined |= mark.m_Attributes;
					if (!sources.empty())
						sources += ", ";
					sources += mark.m_FileName;
				}

				ImGui::TableNextColumn();
				ImGui::TextColored(Orange, "%s", DescribeAttributes(combined).c_str());

				ImGui::TableNextColumn();
				ImGui::TextColored(Muted, "%s", sources.c_str());

				ImGui::PopID();
			}

			ImGui::EndTable();
		}
	}
	ImGui::End();
}
