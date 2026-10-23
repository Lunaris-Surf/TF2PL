#ifdef TF2PL_COMPILE_LIBRARY
#include "thread_pool.hpp"
#else
#define TF2PL_COMPILE_LIBRARY_INLINE inline
#endif

#ifdef TF2PL_COROUTINES_SUPPORTED

namespace tf2pl
{
	namespace detail::thread_pool_hpp
	{
		struct thread_data
		{
			bool m_IsShuttingDown = false;

			tf2pl::dispatcher m_Dispatcher{ false };
			std::vector<std::thread> m_Threads;
		};
	}

	TF2PL_COMPILE_LIBRARY_INLINE thread_pool::thread_pool() :
		thread_pool(std::thread::hardware_concurrency())
	{
	}

	TF2PL_COMPILE_LIBRARY_INLINE thread_pool::thread_pool(size_t threadCount) :
		m_ThreadData(std::make_shared<thread_data>())
	{
		if (threadCount < 1)
			throw std::invalid_argument("threadCount must be >= 1");

		for (size_t i = 0; i < threadCount; i++)
			m_ThreadData->m_Threads.push_back(std::thread(&ThreadFunc, m_ThreadData));
	}

	TF2PL_COMPILE_LIBRARY_INLINE thread_pool::~thread_pool()
	{
		m_ThreadData->m_IsShuttingDown = true;
		for (auto& thread : m_ThreadData->m_Threads)
			thread.detach();
	}

	TF2PL_COMPILE_LIBRARY_INLINE size_t thread_pool::thread_count() const
	{
		return m_ThreadData->m_Threads.size();
	}

	TF2PL_COMPILE_LIBRARY_INLINE size_t thread_pool::task_count() const
	{
		return m_ThreadData->m_Dispatcher.task_count();
	}

	TF2PL_COMPILE_LIBRARY_INLINE void thread_pool::ThreadFunc(std::shared_ptr<thread_data> data)
	{
		using namespace std::chrono_literals;

		while (!data->m_IsShuttingDown)
		{
			data->m_Dispatcher.wait_tasks_for(1s);

			if (!data->m_IsShuttingDown)
			{
				try
				{
					data->m_Dispatcher.run_for(1s);
				}
				catch (...)
				{
					assert(!"Theoretically we should never get here?");
				}
			}
		}
	}

	TF2PL_COMPILE_LIBRARY_INLINE detail::thread_pool_hpp::dispatcher_task_wrapper thread_pool::co_add_task()
	{
		const auto thisThread = std::this_thread::get_id();

		for (const auto& thread : m_ThreadData->m_Threads)
		{
			if (thread.get_id() == thisThread)
				return detail::thread_pool_hpp::dispatcher_task_wrapper(nullptr); // Already on a thread pool thread
		}

		// Not on a thread pool thread, queue ourselves up with the dispatcher
		return detail::thread_pool_hpp::dispatcher_task_wrapper(m_ThreadData->m_Dispatcher.co_dispatch());
	}

	TF2PL_COMPILE_LIBRARY_INLINE tf2pl::dispatcher::delay_task_t thread_pool::co_delay_until(clock_t::time_point timePoint)
	{
		return m_ThreadData->m_Dispatcher.co_delay_until(timePoint);
	}
	TF2PL_COMPILE_LIBRARY_INLINE tf2pl::dispatcher::delay_task_t thread_pool::co_delay_for(clock_t::duration duration)
	{
		return m_ThreadData->m_Dispatcher.co_delay_for(duration);
	}

	TF2PL_COMPILE_LIBRARY_INLINE detail::thread_pool_hpp::dispatcher_task_wrapper::dispatcher_task_wrapper(tf2pl::dispatcher::dispatch_task_t dispatchTask) :
		m_DispatchTask(std::move(dispatchTask))
	{
	}

	TF2PL_COMPILE_LIBRARY_INLINE detail::thread_pool_hpp::dispatcher_task_wrapper::dispatcher_task_wrapper(std::nullptr_t) noexcept
	{
	}

	TF2PL_COMPILE_LIBRARY_INLINE bool detail::thread_pool_hpp::dispatcher_task_wrapper::await_ready() const
	{
		if (m_DispatchTask.has_value())
			return m_DispatchTask->await_ready();

		return true; // Don't suspend
	}

	TF2PL_COMPILE_LIBRARY_INLINE void detail::thread_pool_hpp::dispatcher_task_wrapper::await_resume() const
	{
		if (m_DispatchTask.has_value())
			m_DispatchTask->await_resume();
	}

	TF2PL_COMPILE_LIBRARY_INLINE bool detail::thread_pool_hpp::dispatcher_task_wrapper::await_suspend(coro::coroutine_handle<> parent)
	{
		assert(!await_ready()); // We should never get here if await_ready() returned false

		return m_DispatchTask.value().await_suspend(parent);
	}
}
#endif
