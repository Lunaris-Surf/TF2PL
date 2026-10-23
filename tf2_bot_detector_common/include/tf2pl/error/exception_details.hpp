#pragma once

#include <utility>

#include <exception>
#include <string>
#include <typeinfo>

#ifndef TF2PL_API
#define TF2PL_API
#endif

namespace tf2pl
{
	struct exception_details;

	class exception_details_handler
	{
	public:
		virtual ~exception_details_handler() = default;

		virtual bool try_handle(const std::exception_ptr& e, exception_details& details) const noexcept = 0;
	};

	struct exception_details
	{
		exception_details() noexcept = default;
		TF2PL_API exception_details(const std::exception_ptr& e);

		const std::type_info* m_Type = nullptr;
		std::string m_Message;
		std::exception_ptr m_Nested; // If this was an std::nested_exception, this is filled in

		TF2PL_API const char* type_name() const noexcept;

		// When this goes out of scope, the associated handler is removed.
		struct [[nodiscard]] handler final
		{
			TF2PL_API handler() = default;
			TF2PL_API ~handler();

			handler(const handler&) = delete;
			handler& operator=(const handler&) = delete;

			TF2PL_API handler(handler&& other) noexcept;
			TF2PL_API handler& operator=(handler&& other) noexcept;

		private:
			friend struct exception_details;
			TF2PL_API explicit handler(const std::type_info* type) noexcept;
			TF2PL_API void release();

			const std::type_info* m_Type = nullptr;
		};

		// Adds a new handler. Returns false if there is already a handler for this type.
		TF2PL_API static handler add_handler(const std::type_info& type, const exception_details_handler& handler);
	};
}

#ifndef TF2PL_COMPILE_LIBRARY
#include "exception_details.inl"
#endif
