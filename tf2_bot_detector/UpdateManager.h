#pragma once

#include "Platform/Platform.h"
#include "ReleaseChannel.h"
#include "Version.h"

#include <tf2pl/error/status.hpp>
#include <tf2pl/reflection/enum.hpp>

#include <memory>

namespace tf2_bot_detector
{
	class Settings;

	// we're gonna rely on github tag_names instead.
	struct BuildInfo
	{
		ReleaseChannel m_ReleaseChannel{};
		Version m_Version{};
		std::string m_GitHubURL;
	};
}

namespace tf2_bot_detector
{
	class IAvailableUpdate
	{
	public:
		IAvailableUpdate(BuildInfo&& bi) : m_BuildInfo(std::move(bi)) {}
		virtual ~IAvailableUpdate() = default;

		BuildInfo m_BuildInfo;
	};

	enum class UpdateStatus
	{
		Unknown = 0,

		// ?
		StateSwitchFailure,

		UpdateCheckDisabled,
		InternetAccessDisabled,

		CheckQueued,
		Checking,

		CheckFailed,
		UpToDate,
		UpdateAvailable,

		// DEPRECATED: I won't be implementing these any time soon 
		UpdateToolRequired,
		UpdateToolDownloading,
		UpdateToolDownloadFailed,
		UpdateToolDownloadSuccess,

		// DEPRECATED: I won't be implementing these any time soon 
		Downloading,
		DownloadFailed,
		DownloadSuccess,

		// DEPRECATED: I won't be implementing these any time soon 
		Updating,
		UpdateFailed,
		UpdateSuccess,
	};

	class IUpdateManager
	{
	public:
		virtual ~IUpdateManager() = default;

		static std::unique_ptr<IUpdateManager> Create(const Settings& settings);

		virtual void QueueUpdateCheck() = 0;

		virtual void Update() = 0;

		virtual tf2pl::status_reader<UpdateStatus> GetUpdateStatus() const = 0;
		virtual const IAvailableUpdate* GetAvailableUpdate() const = 0;
	};
}

TF2PL_ENUM_REFLECT_BEGIN(tf2_bot_detector::UpdateStatus)
	TF2PL_ENUM_REFLECT_VALUE(Unknown)

	TF2PL_ENUM_REFLECT_VALUE(StateSwitchFailure)

	TF2PL_ENUM_REFLECT_VALUE(UpdateCheckDisabled)
	TF2PL_ENUM_REFLECT_VALUE(InternetAccessDisabled)

	TF2PL_ENUM_REFLECT_VALUE(Checking)

	TF2PL_ENUM_REFLECT_VALUE(CheckFailed)
	TF2PL_ENUM_REFLECT_VALUE(UpToDate)
	TF2PL_ENUM_REFLECT_VALUE(UpdateAvailable)

	TF2PL_ENUM_REFLECT_VALUE(UpdateToolRequired)
	TF2PL_ENUM_REFLECT_VALUE(UpdateToolDownloading)
	TF2PL_ENUM_REFLECT_VALUE(UpdateToolDownloadFailed)
	TF2PL_ENUM_REFLECT_VALUE(UpdateToolDownloadSuccess)

	TF2PL_ENUM_REFLECT_VALUE(Downloading)
	TF2PL_ENUM_REFLECT_VALUE(DownloadFailed)
	TF2PL_ENUM_REFLECT_VALUE(DownloadSuccess)

	TF2PL_ENUM_REFLECT_VALUE(Updating)
	TF2PL_ENUM_REFLECT_VALUE(UpdateFailed)
	TF2PL_ENUM_REFLECT_VALUE(UpdateSuccess)
TF2PL_ENUM_REFLECT_END()
