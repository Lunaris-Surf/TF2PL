#pragma once

#include "task.hpp"

#ifdef TF2PL_COROUTINES_SUPPORTED

namespace tf2pl
{
	template<typename T> class shared_future;

	template<typename T>
	class future final : public task<T>
	{
		using super = task<T>;

	public:
		using super::super;

		// No copying
		future(const future&) = delete;
		future& operator=(const future&) = delete;

		// Moving allowed
		future(future&&) noexcept = default;
		future& operator=(future&&) = default;

		shared_future<T> share() noexcept;
	};

	template<typename T>
	class shared_future final : public task<T>
	{
		using super = task<T>;

	public:
		using super::super;
		shared_future(future<T>&& f) : shared_future(f.share()) {}
	};

	template<typename T>
	class promise final : task<T>
	{
		using super = task<T>;

	public:
		promise() : super(new detail::promise<T>()) {}

		using super::valid;

		tf2pl::future<T> get_future() const
		{
			return tf2pl::future<T>(this->get_promise_for_copy());
		}
		tf2pl::task<T> get_task() const
		{
			return tf2pl::task<T>(this->get_promise_for_copy());
		}

		void set_value(T value)
		{
			this->get_promise().template set_state<detail::promise<T>::IDX_VALUE>(std::move(value));
		}
		void set_exception(std::exception_ptr p)
		{
			this->get_promise().template set_state<detail::promise<T>::IDX_EXCEPTION>(p);
		}
	};

	template<typename T>
	shared_future<T> future<T>::share() noexcept
	{
		shared_future<T> retVal(this->get_promise_for_copy());
		this->release();
		return retVal;
	}
}
#else
#include <future>
namespace tf2pl
{
	template<typename T> using promise = std::promise<T>;
	template<typename T> using future = std::future<T>;
	template<typename T> using shared_future = std::shared_future<T>;
}
#endif
