#include "zones.hpp"

#include <format>
#include <stdexcept>
#include <string>

namespace cppref::chrono_time {

using namespace std::chrono;

const time_zone* find_zone(std::string_view name) {
    try {
        return locate_zone(name);
    } catch (const std::runtime_error&) {
        throw UnknownZoneError("unknown time zone: " + std::string(name));
    }
}

ZonedInstant in_zone(Instant instant, std::string_view zone) {
    return ZonedInstant{find_zone(zone), instant};
}

std::string format_in_zone(Instant instant, std::string_view zone) {
    return std::format("{:%Y-%m-%d %H:%M %Z}", in_zone(instant, zone));
}

Instant localize(local_time<milliseconds> wall, std::string_view zone, choose how) {
    return find_zone(zone)->to_sys(wall, how);
}

Instant localize_strict(local_time<milliseconds> wall, std::string_view zone) {
    return find_zone(zone)->to_sys(wall);
}

ZonedInstant add_elapsed(const ZonedInstant& dt, milliseconds delta) {
    return ZonedInstant{dt.get_time_zone(), dt.get_sys_time() + delta};
}

ZonedInstant add_calendar_days(const ZonedInstant& dt, days n) {
    const time_zone* zone = dt.get_time_zone();
    const local_time<milliseconds> moved = dt.get_local_time() + n;
    return ZonedInstant{zone, zone->to_sys(moved, choose::earliest)};
}

milliseconds elapsed_between(Instant start, Instant end) {
    return end - start;
}

}  // namespace cppref::chrono_time
