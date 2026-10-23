#pragma once

#include <cstdint>
#include <ostream>

#if __has_include(<source_location>)
#include <source_location>
#endif

namespace tf2pl
{
#if __cpp_lib_source_location >= 201907

	using source_location = std::source_location;
#define TF2PL_SOURCE_LOCATION_CURRENT() ::tf2pl::source_location::current()
#define TF2PL_SOURCE_LOCATION_AUTO(varName) const ::tf2pl::source_location& varName = ::tf2pl::source_location::current()

#else

	class source_location final
	{
	public:
		constexpr source_location() noexcept = default;
		explicit constexpr source_location(std::uint_least32_t line, const char* fileName, const char* functionName, std::uint_least32_t column = 0) noexcept :
			m_Line(line), m_Column(column), m_FileName(fileName), m_FunctionName(functionName)
		{
		}

#if _MSC_VER >= 1927
		static constexpr source_location current(std::uint_least32_t line = __builtin_LINE(), const char* fileName = __builtin_FILE(),
			const char* functionName = __builtin_FUNCTION(), std::uint_least32_t column = __builtin_COLUMN()) noexcept
		{
			return source_location(line, fileName, functionName, column);
		}
#endif

		constexpr std::uint_least32_t line() const noexcept { return m_Line; }
		constexpr std::uint_least32_t column() const noexcept { return m_Column; }
		constexpr const char* file_name() const noexcept { return m_FileName; }
		constexpr const char* function_name() const noexcept { return m_FunctionName; }

	private:
		std::uint_least32_t m_Line{};
		std::uint_least32_t m_Column{};
		const char* m_FileName = nullptr;
		const char* m_FunctionName = nullptr;
	};

#if _MSC_VER >= 1927
#define TF2PL_SOURCE_LOCATION_CURRENT() ::tf2pl::source_location::current()
#define TF2PL_SOURCE_LOCATION_AUTO(varName) const ::tf2pl::source_location& varName = ::tf2pl::source_location::current()
#else
#define TF2PL_SOURCE_LOCATION_CURRENT() ::tf2pl::source_location(__LINE__, __FILE__, __func__)
#define TF2PL_SOURCE_LOCATION_AUTO(varName) const ::tf2pl::source_location& varName
#endif

#endif

	template<typename CharT, typename Traits>
	inline std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const tf2pl::source_location& location)
	{
		return os << location.file_name() << '(' << location.line() << "):" << location.function_name();
	}
}

// fmt 10+ ships a full fmt::formatter<std::source_location> in <fmt/std.h> (tf2pl::source_location is an
// alias for std::source_location on every supported compiler), so the old custom formatter was removed.
#include <fmt/std.h>
