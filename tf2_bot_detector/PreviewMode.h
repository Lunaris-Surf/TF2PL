#pragma once

#include "Clock.h"
#include "SteamID.h"
#include "Config/PlayerListJSON.h"
#include "Tests/FakeWorldState.h"

#include <list>
#include <memory>
#include <string_view>
#include <unordered_map>

namespace tf2_bot_detector
{
	class IConsoleLine;
	class IModeratorLogic;
	class Settings;

	/// <summary>
	/// Preview mode: explore every part of TF2PL without TF2 running (handy for showcasing it).
	///
	/// While a session exists, TF2BDApplication::GetWorld() and GetModLogic() return the session's sample lobby
	/// and a moderator logic that keeps the sample players' marks in memory. Nothing about the sample players
	/// is written to your player lists, and no game commands are sent (votekicks are refused). Players that are
	/// not part of the sample (e.g. entries you open in Player Lists) still go to the real moderator logic.
	/// </summary>
	class PreviewSession final
	{
	public:
		static constexpr std::string_view LIST_NAME = "preview sample (not saved)";

		PreviewSession(IModeratorLogic& realModLogic, const Settings& settings);
		~PreviewSession();

		PreviewSession(const PreviewSession&) = delete;
		PreviewSession& operator=(const PreviewSession&) = delete;

		/// keeps the sample lobby alive: time passes, scores and pings change, chat comes in
		void Update();

		IWorldState& GetWorld();
		const IWorldState& GetWorld() const;
		IModeratorLogic& GetModLogic();
		const IModeratorLogic& GetModLogic() const;

		/// true for the sample lobby's players
		bool OwnsPlayer(const SteamID& id) const;

		/// marks you have given a sample player (what the "your list" editors show for them)
		PlayerAttributesList GetEditableAttributes(const SteamID& id) const;

		/// sample chat, newest first (same order as the real app's printing lines)
		const std::list<std::shared_ptr<const IConsoleLine>>& GetConsoleLines() const { return m_Lines; }

	private:
		friend class PreviewModeratorLogic;

		void AddChatLine(const SteamID& id, std::string_view message, bool team, bool dead);

		const Settings& m_Settings;
		Testing::FakeWorldState m_World;
		std::unique_ptr<IModeratorLogic> m_ModLogic;
		std::unordered_map<SteamID, PlayerAttributesList> m_Marks;
		std::list<std::shared_ptr<const IConsoleLine>> m_Lines;

		time_point_t m_LastTick{};
		time_point_t m_LastChat{};
		size_t m_TickCount = 0;
		size_t m_ChatIndex = 0;
	};

	/// The sample lobby used by preview mode: Testing::PopulateSampleLobby with fuller Steam data
	/// (profiles, bans, logs, inventories, SourceBans) and, if localID is valid, player 0 replaced by you.
	void PopulatePreviewLobby(Testing::FakeWorldState& world, const SteamID& localID);
}
