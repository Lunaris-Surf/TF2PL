#pragma once

#include <tf2pl/source_location.hpp>

#include <stdexcept>
#include <thread>

namespace tf2pl
{
	class thread_sentinel_exception : public std::runtime_error
	{
	public:
		TF2PL_API thread_sentinel_exception(const tf2pl::source_location& location, const std::thread::id& expectedID);

		TF2PL_API const tf2pl::source_location& location() const;

	private:
		tf2pl::source_location m_Location;
	};

	class thread_sentinel
	{
	public:
		TF2PL_API thread_sentinel() noexcept;

		TF2PL_API void check(TF2PL_SOURCE_LOCATION_AUTO(location)) const;

		TF2PL_API void reset_id() noexcept;
		TF2PL_API void reset_id(const std::thread::id& id) noexcept;

	private:
		std::thread::id m_ExpectedID;
	};
}

#ifndef TF2PL_COMPILE_LIBRARY
#include "thread_sentinel.inl"
#endif
