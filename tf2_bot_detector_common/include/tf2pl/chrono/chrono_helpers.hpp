#pragma once

#include <chrono>
#include <ctime>

#ifndef TF2PL_API
#define TF2PL_API
#endif

namespace tf2pl::chrono
{
	template<typename T = double, typename TRep, typename TPeriod>
	inline constexpr T to_seconds(const std::chrono::duration<TRep, TPeriod>& duration)
	{
		return std::chrono::duration_cast<std::chrono::duration<T>>(duration).count();
	}

	enum class time_zone
	{
		local = -100000,
		utc = 0,
	};

	TF2PL_API std::time_t to_time_t(const std::chrono::system_clock::time_point& t);
	TF2PL_API std::time_t to_time_t(std::tm t, time_zone zone);

	TF2PL_API std::tm to_tm(const std::chrono::system_clock::time_point& t, time_zone zone);
	TF2PL_API std::tm to_tm(const std::time_t& t, time_zone zone);

	TF2PL_API std::chrono::system_clock::time_point to_time_point(const std::tm& t, time_zone zone);
	TF2PL_API std::chrono::system_clock::time_point to_time_point(const std::time_t& t);

	TF2PL_API std::tm current_tm(time_zone zone);
	TF2PL_API std::time_t current_time_t();
	TF2PL_API std::chrono::system_clock::time_point current_time_point();
}

#ifndef TF2PL_COMPILE_LIBRARY
#include "chrono_helpers.inl"
#endif
