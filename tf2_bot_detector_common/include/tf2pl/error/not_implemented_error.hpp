#pragma once

#include <stdexcept>
#include <string>

#include <tf2pl/source_location.hpp>

namespace tf2pl
{
	class not_implemented_error : public std::logic_error
	{
		static std::string format_message(const tf2pl::source_location& location)
		{
			std::string retVal;
			retVal
				.append(location.file_name())
				.append("(")
				.append(std::to_string(+location.line()))
				.append("):")
				.append(location.function_name())
				.append(" not implemented");

			return retVal;
		}

	public:
		explicit not_implemented_error(TF2PL_SOURCE_LOCATION_AUTO(location)) :
			std::logic_error(format_message(location)),
			m_Location(location)
		{
		}

		const tf2pl::source_location& location() const noexcept { return m_Location; }

	private:
		tf2pl::source_location m_Location;
	};
}
