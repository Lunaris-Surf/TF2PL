#pragma once

#include <cassert>

#ifndef TF2PL_API
#define TF2PL_API
#endif

namespace tf2pl
{
#ifdef _WIN32
	// Gets the upper and lower bounds of the current thread's stack
	TF2PL_API void get_current_thread_stack_range(void*& lower, void*& upper);

	TF2PL_API bool is_variable_on_current_stack(const void* var);

	// Checks if a variable is on the current thread's stack
	template<typename T, typename = std::enable_if_t<!std::is_pointer_v<T>>>
	inline bool is_variable_on_current_stack(const T& var)
	{
		return is_variable_on_current_stack((const void*)&var);
	}
	template<typename T>
	inline bool is_variable_on_current_stack(const T* var)
	{
		return is_variable_on_current_stack((const void*)var);
	}
#endif
}

#ifndef TF2PL_COMPILE_LIBRARY
#include "stack_info.inl"
#endif
