#define __STDC__WANT_LIB_EXT1__

#ifdef TF2PL_COMPILE_LIBRARY
#include "chrono_helpers.hpp"
#else
#define TF2PL_COMPILE_LIBRARY_INLINE inline
#endif

#include <cassert>
#include <time.h>

namespace tf2pl::chrono
{
	TF2PL_COMPILE_LIBRARY_INLINE std::time_t to_time_t(const std::chrono::system_clock::time_point& t)
	{
		return std::chrono::system_clock::to_time_t(t);
	}
	TF2PL_COMPILE_LIBRARY_INLINE std::time_t to_time_t(std::tm t, [[maybe_unused]] time_zone zone)
	{
		assert(zone == time_zone::local); // All others unsupported

		auto result = std::mktime(&t);
		assert(result != -1);
		return result;
	}

	TF2PL_COMPILE_LIBRARY_INLINE std::tm to_tm(const std::chrono::system_clock::time_point& t, time_zone zone)
	{
		return to_tm(to_time_t(t), zone);
	}
	TF2PL_COMPILE_LIBRARY_INLINE std::tm to_tm(const std::time_t& t, time_zone zone)
	{
		if (zone == time_zone::local)
		{
#ifdef __STDC_LIB_EXT1__
			if (std::tm retVal{}; localtime_s(&t, &retVal) == 0)
				return retVal;
#elif defined (_MSC_VER)
			if (std::tm retVal{}; localtime_s(&retVal, &t) == 0)
				return retVal;
#else
			if (std::tm* retVal = std::localtime(&t))
				return *retVal;
#endif

		}
		else if (zone == time_zone::utc)
		{
#ifdef __STDC_LIB_EXT1__
			if (std::tm retVal{}; gmtime_s(&t, &retVal) == 0)
				return retVal;
#elif defined (_MSC_VER)
			if (std::tm retVal{}; gmtime_s(&retVal, &t) == 0)
				return retVal;
#else
			if (std::tm* retVal = std::gmtime(&t))
				return *retVal;
#endif
		}

		assert(!"time_t -> tm conversion failed");
		return std::tm{};
	}

	TF2PL_COMPILE_LIBRARY_INLINE std::chrono::system_clock::time_point to_time_point(const std::tm& t, time_zone zone)
	{
		return to_time_point(to_time_t(t, zone));
	}
	TF2PL_COMPILE_LIBRARY_INLINE std::chrono::system_clock::time_point to_time_point(const std::time_t& t)
	{
		return std::chrono::system_clock::from_time_t(t);
	}

	TF2PL_COMPILE_LIBRARY_INLINE std::tm current_tm(time_zone zone)
	{
		return to_tm(current_time_t(), zone);
	}
	TF2PL_COMPILE_LIBRARY_INLINE std::time_t current_time_t()
	{
		return std::time(nullptr);
	}
	TF2PL_COMPILE_LIBRARY_INLINE std::chrono::system_clock::time_point current_time_point()
	{
		return std::chrono::system_clock::now();
	}
}
