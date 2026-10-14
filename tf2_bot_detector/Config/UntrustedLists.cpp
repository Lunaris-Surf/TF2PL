#include "UntrustedLists.h"
#include "Filesystem.h"
#include "Log.h"

#include <nlohmann/json.hpp>
#include <fmt/std.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <mutex>
#include <utility>
#include <sstream>

using namespace tf2_bot_detector;

namespace
{
	// lowercase; matched case-insensitively
	constexpr std::string_view UNTRUSTED_MARKERS[] =
	{
		"vorobey-hackerpolice",
		"vorobey_hackerpolice",
		"vorobeyhackerpolice",
		"sterling",
		"shadefall",
		"shadefall-hunters",
		"shadefall_hunters",
		"shadefallhunters",
		"shitlist",
		"shit-list",
		"shit_list",
	};

	std::mutex s_ReportMutex;
	UntrustedLists::Report s_Report;

	bool EqualsNoCase(char a, char b)
	{
		return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
	}

	bool ContainsNoCase(std::string_view haystack, std::string_view needle)
	{
		return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(), EqualsNoCase) != haystack.end();
	}

	bool StartsWithNoCase(std::string_view s, std::string_view prefix)
	{
		return s.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), s.begin(), EqualsNoCase);
	}

	bool EndsWithNoCase(std::string_view s, std::string_view suffix)
	{
		return s.size() >= suffix.size() && std::equal(suffix.begin(), suffix.end(), s.end() - suffix.size(), EqualsNoCase);
	}

	std::string ReadWholeFile(const std::filesystem::path& path)
	{
		std::ifstream file(path, std::ios::binary);
		std::ostringstream ss;
		ss << file.rdbuf();
		return ss.str();
	}

	void AddUnique(std::vector<std::filesystem::path>& vec, const std::filesystem::path& path)
	{
		if (std::find(vec.begin(), vec.end(), path) == vec.end())
			vec.push_back(path);
	}

	/// <summary>
	/// delete the file; if that fails, rename it so it no longer matches playerlist*.json and won't load.
	/// </summary>
	bool RemoveUntrustedFile(const std::filesystem::path& path)
	{
		std::error_code ec;
		if (std::filesystem::remove(path, ec))
		{
			LogWarning("Deleted untrusted player list {} (known to contain incorrect/fake cheater entries)", path);
			return true;
		}

		auto disabled = path;
		disabled += ".untrusted-disabled";
		std::error_code renameEc;
		std::filesystem::rename(path, disabled, renameEc);
		if (!renameEc)
		{
			LogWarning("Could not delete untrusted player list {} ({}); renamed it to {} so it will not be loaded",
				path, ec.message(), disabled);
			return true;
		}

		LogError("Failed to delete or disable untrusted player list {}: {} / {}", path, ec.message(), renameEc.message());
		return false;
	}
}

bool UntrustedLists::ContainsUntrustedMarker(std::string_view text)
{
	for (const auto& marker : UNTRUSTED_MARKERS)
	{
		if (ContainsNoCase(text, marker))
			return true;
	}
	return false;
}

bool UntrustedLists::IsUntrustedListJson(const nlohmann::json& json)
{
	if (!json.is_object())
		return false;

	auto fileInfo = json.find("file_info");
	if (fileInfo == json.end())
		return false;

	// title, description, update_url, authors... any of them naming the list is enough
	return ContainsUntrustedMarker(fileInfo->dump());
}

void UntrustedLists::ScreenPlayerLists()
{
	constexpr std::filesystem::directory_options options =
		std::filesystem::directory_options::skip_permission_denied | std::filesystem::directory_options::follow_directory_symlink;

	Report found;
	std::vector<std::filesystem::path> exports;

	try
	{
		for (const auto& entry : IFilesystem::Get().IterateDir("cfg", false, options))
		{
			std::error_code ec;
			if (!entry.is_regular_file(ec))
				continue;

			const auto path = entry.path();
			const std::string filename = path.filename().string();
			if (!StartsWithNoCase(filename, "playerlist") || !EndsWithNoCase(filename, ".json"))
				continue;

			// distributed as playerlist.vorobey-hackerpolice.json
			if (ContainsUntrustedMarker(filename))
			{
				if (RemoveUntrustedFile(path))
					AddUnique(found.m_Deleted, path);
				continue;
			}

			std::string contents;
			try
			{
				contents = ReadWholeFile(path);
			}
			catch (const std::exception& e)
			{
				LogException(MH_SOURCE_LOCATION_CURRENT(), e, "Failed to read {} while checking for untrusted lists", path);
				continue;
			}

			if (StartsWithNoCase(filename, "playerlist.export."))
				exports.push_back(path);

			if (!ContainsUntrustedMarker(contents))
				continue;

			// The marker is in there somewhere. If the file itself *is* the list (renamed copy), delete it;
			// otherwise it's a list carrying entries from it, which the user should know is unreliable.
			bool isUntrustedList = false;
			try
			{
				isUntrustedList = IsUntrustedListJson(nlohmann::json::parse(contents));
			}
			catch (...)
			{
				// unparseable; leave it to the normal loader to complain, but still flag it
			}

			if (isUntrustedList)
			{
				if (RemoveUntrustedFile(path))
					AddUnique(found.m_Deleted, path);
			}
			else
			{
				LogWarning("Player list {} contains entries from an untrusted list; its cheater marks may be unreliable", path);
				AddUnique(found.m_Contaminated, path);
			}
		}
	}
	catch (const std::exception& e)
	{
		LogException(MH_SOURCE_LOCATION_CURRENT(), e, "Failed to scan cfg for untrusted player lists");
	}

	// Combined exports don't record where entries came from, so if the untrusted list was installed,
	// any export made while it was loaded may contain its entries.
	if (!found.m_Deleted.empty())
	{
		for (const auto& path : exports)
		{
			if (std::find(found.m_Contaminated.begin(), found.m_Contaminated.end(), path) == found.m_Contaminated.end())
				AddUnique(found.m_PossiblyContaminatedExports, path);
		}
	}

	if (found.empty())
		return;

	std::lock_guard lock(s_ReportMutex);
	for (const auto& p : found.m_Deleted)
		AddUnique(s_Report.m_Deleted, p);
	for (const auto& p : found.m_Contaminated)
		AddUnique(s_Report.m_Contaminated, p);
	for (const auto& p : found.m_PossiblyContaminatedExports)
		AddUnique(s_Report.m_PossiblyContaminatedExports, p);
}

UntrustedLists::Report UntrustedLists::TakeReport()
{
	std::lock_guard lock(s_ReportMutex);
	return std::exchange(s_Report, {});
}

bool UntrustedLists::HasReport()
{
	std::lock_guard lock(s_ReportMutex);
	return !s_Report.empty();
}
