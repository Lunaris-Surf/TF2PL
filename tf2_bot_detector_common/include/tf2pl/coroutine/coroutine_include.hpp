#pragma once

#if defined(__INTELLISENSE__) || __has_include(<coroutine>)
#include <coroutine>
#define TF2PL_COROUTINES_SUPPORTED 1
namespace tf2pl::detail
{
	namespace coro = std;
}
#elif __has_include(<experimental/coroutine>)
#include <experimental/coroutine>
#define TF2PL_COROUTINES_SUPPORTED 1
namespace tf2pl::detail
{
	namespace coro = std::experimental;
}
#endif
