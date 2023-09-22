#pragma once

#include <date/iso_week.h>
#include <date/ptz.h>
#include <date/tz.h>

#include <ctime>
#include <string>

inline uint64_t currentDayStartTs(const std::string& timeZone = "Europe/Moscow") {
	using namespace std::chrono;
	auto zone = date::locate_zone(timeZone);

	auto zonedTime = date::make_zoned(zone, system_clock::now());
	auto startDay = floor<date::days>(zonedTime.get_local_time());

	return seconds(zone->to_sys(startDay).time_since_epoch()).count();
}

inline uint64_t localDayStartTs(uint64_t ts, const std::string& timeZone = "Europe/Moscow") {
	using namespace std::chrono;
	auto zone = date::locate_zone(timeZone);

	auto zonedTime = date::make_zoned(zone, date::sys_time<seconds>(seconds(ts)));
	auto startDay = floor<date::days>(zonedTime.get_local_time());

	return seconds(zone->to_sys(startDay).time_since_epoch()).count();
}
