#pragma once

#include <tf2pl/coroutine/task.hpp>

#ifdef TF2PL_COROUTINES_SUPPORTED

#ifndef TF2PL_API
#define TF2PL_API
#endif

#include "dispatcher.hpp"

#include <memory>
#include <optional>

namespace tf2pl
{
	namespace detail::thread_pool_hpp
	{
		struct thread_data;

		struct dispatcher_task_wrapper
		{
			explicit dispatcher_task_wrapper(tf2pl::dispatcher::dispatch_task_t dispatchTask);
			explicit dispatcher_task_wrapper(std::nullptr_t) noexcept;

			TF2PL_API bool await_ready() const;
			TF2PL_API void await_resume() const;
			TF2PL_API bool await_suspend(coro::coroutine_handle<> parent);

		private:
			std::optional<tf2pl::dispatcher::dispatch_task_t> m_DispatchTask;
		};
	}

	class thread_pool final
	{
		using thread_data = detail::thread_pool_hpp::thread_data;

	public:
		using clock_t = tf2pl::dispatcher::clock_t;

		TF2PL_API thread_pool();
		TF2PL_API thread_pool(size_t threadCount);
		TF2PL_API ~thread_pool();

		TF2PL_API detail::thread_pool_hpp::dispatcher_task_wrapper co_add_task();

		TF2PL_API tf2pl::dispatcher::delay_task_t co_delay_until(clock_t::time_point timePoint);
		TF2PL_API tf2pl::dispatcher::delay_task_t co_delay_for(clock_t::duration duration);

		template<typename TFunc, typename... TArgs>
		tf2pl::task<std::invoke_result_t<TFunc, TArgs...>> add_task(TFunc func, TArgs... args)
		{
			co_await co_add_task();

			co_return func(std::move(args)...);
		}

		TF2PL_API size_t thread_count() const;
		TF2PL_API size_t task_count() const;

	private:
		std::shared_ptr<thread_data> m_ThreadData;

		static void ThreadFunc(std::shared_ptr<thread_data> data);
	};
}

#ifndef TF2PL_COMPILE_LIBRARY
#include "thread_pool.inl"
#endif

#endif
