#pragma once

#include <tf2pl/reflection/enum.hpp>

namespace tf2_bot_detector
{
	// DO NOT FORWARD DECLARE (implicit cast in fmt::format from enum class -> int)
	enum class ReleaseChannel
	{
		None = -1, // Don't auto update
		Public,
		Preview,
		Nightly,
	};
}

TF2PL_ENUM_REFLECT_BEGIN(tf2_bot_detector::ReleaseChannel)
	TF2PL_ENUM_REFLECT_VALUE(None)
	TF2PL_ENUM_REFLECT_VALUE(Public)
	TF2PL_ENUM_REFLECT_VALUE(Preview)
	TF2PL_ENUM_REFLECT_VALUE(Nightly)
TF2PL_ENUM_REFLECT_END()
