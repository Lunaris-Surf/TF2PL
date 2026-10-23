#include "Clock.h"

#include <tf2pl/chrono/chrono_helpers.hpp>

using namespace tf2_bot_detector;

tm tf2_bot_detector::ToTM(const time_point_t& ts)
{
	return tf2pl::chrono::to_tm(ts, tf2pl::chrono::time_zone::local);
}

tm tf2_bot_detector::GetLocalTM()
{
	return tf2pl::chrono::current_tm(tf2pl::chrono::time_zone::local);
}

time_point_t tf2_bot_detector::GetLocalTimePoint()
{
	return tf2pl::chrono::current_time_point();
}
