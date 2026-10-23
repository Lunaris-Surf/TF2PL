#pragma once

#if __has_include(<tf2pl/coroutine/future.hpp>)
#include <tf2pl/coroutine/future.hpp>
namespace tf2pl::detail::async_hpp
{
	template<typename T> using promise_type = tf2pl::promise<T>;
	template<typename T> using future_type = tf2pl::future<T>;
}
#else
#include <future>
namespace tf2pl::detail::async_hpp
{
	template<typename T> using promise_type = std::promise<T>;
	template<typename T> using future_type = std::future<T>;
}
#endif

#include <thread>
#include <type_traits>

namespace tf2pl
{
	template<typename TFunc, typename... TArgs>
	auto async(TFunc&& func, TArgs&&... args)
	{
		using ret_type = std::invoke_result_t<TFunc, TArgs...>;
		using promise_type = detail::async_hpp::promise_type<ret_type>;
		promise_type promise;
		auto future = promise.get_future();

		std::thread t([](promise_type promise, TFunc&& func, TArgs&&... args)
			{
				try
				{
					promise.set_value(func(std::forward<TArgs>(args)...));
				}
				catch (...)
				{
					promise.set_exception(std::current_exception());
				}
			}, std::move(promise), std::forward<TFunc>(func), std::forward<TArgs>(args)...);

		t.detach();

		return future;
	}
}
