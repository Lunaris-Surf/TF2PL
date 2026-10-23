#pragma once

#include <thread>

namespace tf2pl
{
	inline static const std::thread::id main_thread_id = std::this_thread::get_id();

	inline bool is_main_thread()
	{
		return main_thread_id == std::this_thread::get_id();
	}
}
