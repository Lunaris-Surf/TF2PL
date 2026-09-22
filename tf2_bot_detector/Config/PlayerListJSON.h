#pragma once

#include "ConfigHelpers.h"
#include "ModeratorLogic.h"
#include "SteamID.h"

#include <mh/coroutine/generator.hpp>
#include <nlohmann/json_fwd.hpp>

#include <bitset>
#include <chrono>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>

namespace tf2_bot_detector
{
	class Settings;

	/// <summary>
	/// a player's attributes:
	/// exploiter, cheater, etc.
	///
	/// Custom attributes use the custom: namespace in the same JSON array.
	/// </summary>
	enum class PlayerAttribute
	{
		Cheater,
		Suspicious,
		Exploiter,
		Racist,

		SuspectedCheater,
		Blacklisted,
		VACBanned,
		GameBanned,
		SourceBanned,
		Pedophilia,

		COUNT,
	};

	struct PlayerAttributesList final
	{
		using bits_t = std::bitset<size_t(PlayerAttribute::COUNT)>;

		PlayerAttributesList() = default;
		explicit PlayerAttributesList(const bits_t& bits) : m_Bits(bits) {}
		PlayerAttributesList(const std::initializer_list<PlayerAttribute>& attributes);
		PlayerAttributesList(PlayerAttribute attribute);

		static constexpr size_t size() { return size_t(PlayerAttribute::COUNT); }
		bool HasAttribute(PlayerAttribute attribute) const { return m_Bits.test(size_t(attribute)); }
		bool SetAttribute(PlayerAttribute attribute, bool set = true);

		friend PlayerAttributesList operator|(const PlayerAttributesList& lhs, const PlayerAttributesList& rhs)
		{
			auto result = lhs;
			result |= rhs;
			return result;
		}
		friend PlayerAttributesList& operator|=(PlayerAttributesList& lhs, const PlayerAttributesList& rhs)
		{
			lhs.m_Bits |= rhs.m_Bits;
			lhs.m_CustomTags.insert(rhs.m_CustomTags.begin(), rhs.m_CustomTags.end());
			return lhs;
		}
		friend PlayerAttributesList operator&(const PlayerAttributesList& lhs, const PlayerAttributesList& rhs)
		{
			auto result = lhs;
			result &= rhs;
			return result;
		}
		friend PlayerAttributesList& operator&=(PlayerAttributesList& lhs, const PlayerAttributesList& rhs)
		{
			lhs.m_Bits &= rhs.m_Bits;
			std::erase_if(lhs.m_CustomTags, [&](const auto& tag) { return !rhs.m_CustomTags.contains(tag); });
			return lhs;
		}

		bool operator==(const PlayerAttributesList&) const = default;

		bool empty() const { return m_Bits.none() && m_CustomTags.empty(); }
		std::size_t count() const { return m_Bits.count() + m_CustomTags.size(); }
		explicit operator bool() const { return !empty(); }

		static bool IsValidCustomTag(const std::string& tag);
		bool SetCustomTag(const std::string& tag, bool set = true);
		const std::set<std::string>& GetCustomTags() const { return m_CustomTags; }

	private:
		bits_t m_Bits;
		std::set<std::string> m_CustomTags;
	};

	void to_json(nlohmann::json& j, const PlayerAttributesList& d);
	void from_json(const nlohmann::json& j, PlayerAttributesList& d);

	inline PlayerAttributesList operator|(PlayerAttribute lhs, PlayerAttribute rhs)
	{
		return PlayerAttributesList({ lhs, rhs });
	}

	struct PlayerListData
	{
		PlayerListData(const SteamID& id);
		~PlayerListData();

		constexpr SteamID GetSteamID() const { return m_SteamID; }

		PlayerAttributesList m_SavedAttributes;
		PlayerAttributesList m_TransientAttributes;
		PlayerAttributesList GetAttributes() const { return m_SavedAttributes | m_TransientAttributes; }

		struct LastSeen
		{
			std::chrono::system_clock::time_point m_Time;
			std::string m_PlayerName;

			static std::optional<LastSeen> Latest(
				const std::optional<LastSeen>& lhs, const std::optional<LastSeen>& rhs);

			constexpr bool operator==(const LastSeen& other) const { return m_Time == other.m_Time; }
			constexpr auto operator<=>(const LastSeen& other) const
			{
				return m_Time.time_since_epoch().count() <=> other.m_Time.time_since_epoch().count();
			}
		};
		std::optional<LastSeen> m_LastSeen;

		// TODO: vector<std::string>
		std::vector<nlohmann::json> m_Proof;
		void addProof(std::string reason);
		bool proofExists(std::string reason);

		bool operator==(const PlayerListData&) const;

	private:
		SteamID m_SteamID;
	};

	enum class ModifyPlayerResult
	{
		NoChanges,
		FileSaved,
	};

	enum class ModifyPlayerAction
	{
		NoChanges,
		Modified,
	};

	/*
	template<typename T> concept ModifyPlayerCallback = requires(T x)
	{
#ifndef __INTELLISENSE__
		std::invocable<T, PlayerListData&>;
		{ x(std::declval<PlayerListData>()) } -> std::same_as<ModifyPlayerAction>;
#endif
	};
	*/

	using ConfigFileName = std::string;
	struct PlayerMarks final
	{
		struct Mark final
		{
			Mark(const PlayerAttributesList& attr, const ConfigFileName& fileName) :
				m_Attributes(attr), m_FileName(fileName)
			{
			}

			PlayerAttributesList m_Attributes;
			ConfigFileName m_FileName;
		};

		bool Has(const PlayerAttributesList& attr) const;

		bool empty() const { return m_Marks.empty(); }
		explicit operator bool() const { return !empty(); }
		bool operator!() const { return empty(); }

		auto begin() { return m_Marks.begin(); }
		auto end() { return m_Marks.end(); }
		auto begin() const { return m_Marks.begin(); }
		auto end() const { return m_Marks.end(); }
		std::vector<Mark> m_Marks;
	};

	class PlayerListJSON final
	{
	public:
		PlayerListJSON(const Settings& settings);

		bool LoadFiles();
		void SaveFiles() const;

		mh::generator<std::pair<const ConfigFileName&, const PlayerListData&>>
			FindPlayerData(const SteamID& id) const;
		/// <summary>
		/// Yields every player across all loaded playerlists.
		/// </summary>
		mh::generator<std::pair<const ConfigFileName&, const PlayerListData&>>
			GetAllPlayerData() const;
		mh::generator<std::pair<const ConfigFileName&, PlayerAttributesList>>
			FindPlayerAttributes(const SteamID& id, AttributePersistence persistence = AttributePersistence::Any) const;
		PlayerMarks GetPlayerAttributes(const SteamID& id) const;
		PlayerMarks HasPlayerAttributes(const SteamID& id, const PlayerAttributesList& attributes,
			AttributePersistence persistence = AttributePersistence::Any) const;

		ModifyPlayerResult ModifyPlayer(const SteamID& id,
			const std::function<ModifyPlayerAction(PlayerListData& data)>& func);

		std::set<std::string> GetCustomTags() const;
		PlayerAttributesList GetEditablePlayerAttributes(const SteamID& id) const;

		size_t GetPlayerCount() const { return m_CFGGroup.size(); }

	private:
		const Settings* m_Settings = nullptr;

		ModifyPlayerAction OnPlayerDataChanged(PlayerListData& data);

		using PlayerMap_t = std::map<SteamID, PlayerListData>;

		struct PlayerListFile final : public SharedConfigFileBase
		{
			void ValidateSchema(const ConfigSchemaInfo& schema) const override;
			void Deserialize(const nlohmann::json& json) override;
			void Serialize(nlohmann::json& json) const override;

			size_t size() const { return m_Players.size(); }

			PlayerListData& GetOrAddPlayer(const SteamID& id);

			PlayerMap_t m_Players;
		};

		static constexpr int PLAYERLIST_SCHEMA_VERSION = 3;

		struct ConfigFileGroup final : public ConfigFileGroupBase<PlayerListFile, std::vector<std::pair<ConfigFileName, PlayerMap_t>>>
		{
			using BaseClass = ConfigFileGroupBase;

			using ConfigFileGroupBase::ConfigFileGroupBase;
			void CombineEntries(BaseClass::collection_type& map, const PlayerListFile& file) const override;
			std::string GetBaseFileName() const override { return "playerlist"; }

		} m_CFGGroup;

	public:
		// this seems like a bad idea, idk why.
		ConfigFileGroup& GetConfigFileGroup() { return m_CFGGroup; }

		friend class PlayerListManagementWindow;
	};

	std::string to_string(const PlayerAttribute& d);
	void to_json(nlohmann::json& j, const PlayerAttribute& d);
	void from_json(const nlohmann::json& j, PlayerAttribute& d);
}

MH_ENUM_REFLECT_BEGIN(tf2_bot_detector::PlayerAttribute)
	MH_ENUM_REFLECT_VALUE(Cheater)
	MH_ENUM_REFLECT_VALUE(Exploiter)
	MH_ENUM_REFLECT_VALUE(Racist)
	MH_ENUM_REFLECT_VALUE(Suspicious)
	MH_ENUM_REFLECT_VALUE(SuspectedCheater)
	MH_ENUM_REFLECT_VALUE(Blacklisted)
	MH_ENUM_REFLECT_VALUE(VACBanned)
	MH_ENUM_REFLECT_VALUE(GameBanned)
	MH_ENUM_REFLECT_VALUE(SourceBanned)
	MH_ENUM_REFLECT_VALUE(Pedophilia)
MH_ENUM_REFLECT_END()

template<typename CharT>
struct fmt::formatter<tf2_bot_detector::PlayerAttributesList, CharT>
{
	constexpr auto parse(basic_format_parse_context<CharT>& ctx) const noexcept { return ctx.begin(); }

	template<typename FormatContext>
	auto format(const tf2_bot_detector::PlayerAttributesList& list, FormatContext& ctx) const
	{
		bool printed = false;
		auto it = ctx.out();
		for (size_t i = 0; i < list.size(); i++)
		{
			const auto thisAttr = tf2_bot_detector::PlayerAttribute(i);
			if (!list.HasAttribute(thisAttr))
				continue;

			if (printed)
				it = fmt::format_to(it, FMT_STRING(", "));

			it = fmt::format_to(it, FMT_STRING("{:v}"), mh::enum_fmt(thisAttr));
			printed = true;
		}

		for (const auto& tag : list.GetCustomTags())
		{
			if (printed) it = fmt::format_to(it, FMT_STRING(", "));
			it = fmt::format_to(it, FMT_STRING("{}"), tag);
			printed = true;
		}

		return it;
	}
};

template<typename CharT>
struct fmt::formatter<tf2_bot_detector::PlayerMarks::Mark, CharT>
{
	constexpr auto parse(basic_format_parse_context<CharT>& ctx) const noexcept { return ctx.begin(); }

	template<typename FormatContext>
	auto format(const tf2_bot_detector::PlayerMarks::Mark& mark, FormatContext& ctx) const
	{
		return fmt::format_to(ctx.out(), FMT_STRING("{} ({})"), fmt::streamed(std::quoted(mark.m_FileName)), mark.m_Attributes);
	}
};

template<typename CharT>
struct fmt::formatter<tf2_bot_detector::PlayerMarks, CharT>
{
	constexpr auto parse(basic_format_parse_context<CharT>& ctx) const noexcept { return ctx.begin(); }

	template<typename FormatContext>
	auto format(const tf2_bot_detector::PlayerMarks& marks, FormatContext& ctx) const
	{
		auto it = ctx.out();

		for (auto& mark : marks)
			it = fmt::format_to(it, FMT_STRING("\n\t - {}"), mark);

		return it;
	}
};
