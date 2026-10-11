#pragma once

#include "Clock.h"

namespace tf2_bot_detector
{
	class IAction;
	class IActionManager;
	class Settings;

	class IActionGenerator
	{
	public:
		virtual ~IActionGenerator() = default;

		virtual bool Execute(IActionManager& manager) = 0;
	};

	class IPeriodicActionGenerator : public IActionGenerator
	{
	public:
		virtual ~IPeriodicActionGenerator() = default;

		virtual duration_t GetInterval() const = 0;
		virtual duration_t GetInitialDelay() const { return {}; }

		bool Execute(IActionManager& manager) override final;

	protected:
		[[nodiscard]] virtual bool ExecuteImpl(IActionManager& manager) = 0;

	private:
		time_point_t m_LastRunTime{};
	};

	// Legacy status/ping polling. Player data polling now lives in RCONActionManager
	// (hybrid g15_dumpplayer + status); kept for reference, not registered.
	class StatusUpdateActionGenerator final : public IPeriodicActionGenerator
	{
	public:
		duration_t GetInterval() const override;

	protected:
		bool ExecuteImpl(IActionManager& manager) override;

	private:
		bool m_NextShort = false;
		bool m_NextPing = false;
	};

	class ConfigActionGenerator final : public IPeriodicActionGenerator
	{
	public:
		duration_t GetInterval() const override;

	protected:
		bool ExecuteImpl(IActionManager& manager) override;
	};

	class LobbyDebugActionGenerator final : public IPeriodicActionGenerator
	{
	public:
		explicit LobbyDebugActionGenerator(const Settings& settings) : m_Settings(&settings) {}

		// With G15 we already get teams directly, so the lobby dump is only a slower cross-check.
		duration_t GetInterval() const override;

	protected:
		bool ExecuteImpl(IActionManager& manager) override;

	private:
		const Settings* m_Settings = nullptr;
	};
}
