#include "StreamerMode.h"
#include "SteamID.h"
#include "Config/Settings.h"

using namespace tf2_bot_detector;

bool StreamerMode::IsEnabled(const Settings& settings)
{
	return settings.m_StreamerMode.m_Enabled;
}

bool StreamerMode::IsMasked(const Settings& settings, const SteamID& id)
{
	if (!settings.m_StreamerMode.m_Enabled || !id.IsValid())
		return false;

	return id == settings.GetLocalSteamID();
}

std::string StreamerMode::GetName(const Settings& settings)
{
	const auto& name = settings.m_StreamerMode.m_Name;
	return name.empty() ? std::string(DEFAULT_NAME) : name;
}

std::string StreamerMode::GetSteamIDText(const Settings& settings)
{
	const auto& id = settings.m_StreamerMode.m_SteamID;
	return id.empty() ? std::string(HIDDEN_STEAMID) : id;
}

std::string StreamerMode::DisplayName(const Settings& settings, const SteamID& id, std::string_view realName)
{
	return IsMasked(settings, id) ? GetName(settings) : std::string(realName);
}

std::string StreamerMode::DisplaySteamID(const Settings& settings, const SteamID& id)
{
	return IsMasked(settings, id) ? GetSteamIDText(settings) : id.str();
}

std::string StreamerMode::DisplaySteamID64(const Settings& settings, const SteamID& id)
{
	return IsMasked(settings, id) ? GetSteamIDText(settings) : std::to_string(id.GetSteamID64());
}
