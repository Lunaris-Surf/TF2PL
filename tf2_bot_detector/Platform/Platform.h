#pragma once

#include "ReleaseChannel.h"
#include "SteamID.h"
#include "Version.h"

#include <tf2pl/coroutine/task.hpp>
#include <tf2pl/reflection/enum.hpp>

#include <filesystem>
#include <future>
#include <string>
#include <variant>

#ifdef _WIN32
#define PLATFORM_EXECUTABLE(n) n ".exe"
#define STEAM_BIN_DIR(n) n
#else
#define PLATFORM_EXECUTABLE(n) n
#define STEAM_BIN_DIR(n) "ubuntu12_32/" n
#endif

namespace tf2_bot_detector
{
	class IHTTPClient;
	struct BuildInfo;

	inline namespace Platform
	{
		// NOTE: more like "GetDefaultSteamDir"
		std::filesystem::path GetCurrentSteamDir();
		SteamID GetCurrentActiveSteamID();

		std::filesystem::path GetCurrentExeDir();
		std::filesystem::path GetRootLocalAppDataDir();
		std::filesystem::path GetRootRoamingAppDataDir();
		std::filesystem::path GetLegacyAppDataDir();
		std::filesystem::path GetRootTempDataDir();
		bool IsPortAvailable(uint16_t port);

		bool IsDebuggerAttached();

		enum class OS
		{
			Windows,
			Linux,
		};
		OS GetOS();

		enum class Arch
		{
			x86,
			x64,
		};
		Arch GetArch();

		namespace InstallUpdate
		{
			struct Success {};

			// We think we've started the update, but there's no way to determine if it succeeded or not.
			struct StartedNoFeedback {};

			struct NeedsUpdateTool
			{
				std::string m_UpdateToolArgs;
			};

			using Result = std::variant<
				Success,
				StartedNoFeedback,
				NeedsUpdateTool>;
		};

		// As opposed to portable,
		// this always returns false, as msix "installed" is a can of worm that i do not want to bother;
		// and is getting removed.
		bool CanInstallUpdate(const BuildInfo& bi);

		bool IsInstalled();

		namespace Processes
		{
			bool IsTF2Running();
			tf2pl::task<std::vector<std::string>> GetTF2CommandLineArgsAsync();
			bool IsSteamRunning();
			bool IsProcessRunning(const std::string_view& processName);

			void Launch(const std::filesystem::path& executable, const std::vector<std::string>& args = {},
				bool elevated = false, const std::filesystem::path& workingDir = {});
			void Launch(const std::filesystem::path& executable, const std::string_view& args = {},
					bool elevated = false, const std::filesystem::path& workingDir = {});
			int GetCurrentProcessID();

			size_t GetCurrentRAMUsage();
		}

		namespace Shell
		{
			std::vector<std::string> SplitCommandLineArgs(const std::string_view& cmdline);
			std::filesystem::path BrowseForFolderDialog();
			/// pick an image file (png/jpg/bmp/tga/gif); empty if cancelled
			std::filesystem::path BrowseForImageDialog();
			void ExploreToAndSelect(std::filesystem::path path);

			void ExploreTo(const std::filesystem::path& path);
			void OpenURL(const char* url);
			inline void OpenURL(const std::string& url) { return OpenURL(url.c_str()); }
		}

		namespace ErrorCodes
		{
			extern const std::error_code PRIVILEGE_NOT_HELD;
		}
	}
}

TF2PL_ENUM_REFLECT_BEGIN(tf2_bot_detector::Platform::OS)
	TF2PL_ENUM_REFLECT_VALUE(Windows)
	TF2PL_ENUM_REFLECT_VALUE(Linux)
TF2PL_ENUM_REFLECT_END()

TF2PL_ENUM_REFLECT_BEGIN(tf2_bot_detector::Platform::Arch)
	TF2PL_ENUM_REFLECT_VALUE(x86)
	TF2PL_ENUM_REFLECT_VALUE(x64)
TF2PL_ENUM_REFLECT_END()
