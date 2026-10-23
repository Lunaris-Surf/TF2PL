#ifdef TF2PL_COROUTINES_SUPPORTED

#ifdef TF2PL_COMPILE_LIBRARY
#include <tf2pl/coroutine/thread.hpp>
#endif

#ifndef TF2PL_COMPILE_LIBRARY_INLINE
#define TF2PL_COMPILE_LIBRARY_INLINE inline
#endif

#if __has_include (<tf2pl/concurrency/main_thread.hpp>)
#include <tf2pl/concurrency/main_thread.hpp>
namespace tf2pl::detail::coroutine::thread_hpp
{
	TF2PL_COMPILE_LIBRARY_INLINE bool is_main_thread()
	{
		return tf2pl::is_main_thread();
	}
}
#else
namespace tf2pl::detail::coroutine::thread_hpp
{
	$TF2PL_COMPILE_LIBRARY_INLINE static std::thread::id s_MainThreadID = std::this_thread::get_id();
	$TF2PL_COMPILE_LIBRARY_INLINE bool is_main_thread()
	{
		return std::this_thread::get_id() == s_MainThreadID;
	}
}
#endif

#include <cassert>
#include <thread>

namespace tf2pl::detail::coroutine::thread_hpp
{
	TF2PL_COMPILE_LIBRARY_INLINE bool task::await_suspend(coro::coroutine_handle<> waiter)
	{
		if (m_Flags == co_create_thread_flags::none ||
			(m_Flags == co_create_thread_flags::off_main_thread && is_main_thread()))
		{
			std::thread t([](coro::coroutine_handle<> waiter) { waiter.resume(); }, waiter);
			t.detach();

			// always suspend
			return true;
		}
		else
		{
			// resume on current thread
			return false;
		}
	}

	//$TF2PL_COMPILE_LIBRARY_INLINE task promise::get_return_object()
	//{
	//	return {};
	//}

	TF2PL_COMPILE_LIBRARY_INLINE task::task(co_create_thread_flags flags) :
		m_Flags(flags)
	{
	}
}

TF2PL_COMPILE_LIBRARY_INLINE tf2pl::detail::coroutine::thread_hpp::task tf2pl::co_create_thread()
{
	return {};
}

TF2PL_COMPILE_LIBRARY_INLINE tf2pl::detail::coroutine::thread_hpp::task tf2pl::co_create_background_thread()
{
	return { detail::coroutine::thread_hpp::co_create_thread_flags::off_main_thread };
}

#endif
