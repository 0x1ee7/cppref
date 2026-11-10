// Part 3: time zones, wall-clock vs elapsed arithmetic.
#pragma once

#include "common.hpp"

#include <string>
#include <string_view>

namespace cppref::chrono_time {

// Looks up an IANA zone name (for example "Europe/Berlin").
// Throws UnknownZoneError if the name is not in the tzdb.
const std::chrono::time_zone* find_zone(std::string_view name);

// The instant displayed as wall time in `zone`.
ZonedInstant in_zone(Instant instant, std::string_view zone);

// "%Y-%m-%d %H:%M %Z" in `zone`, e.g. "2026-03-29 14:00 CEST".
std::string format_in_zone(Instant instant, std::string_view zone);

// Resolves an ambiguous wall time (one that occurs twice) with `how`:
// choose::earliest picks the first occurrence, choose::latest the second.
Instant localize(std::chrono::local_time<std::chrono::milliseconds> wall,
                 std::string_view zone, std::chrono::choose how);

// Strict resolution: throws std::chrono::ambiguous_local_time or
// std::chrono::nonexistent_local_time from the standard library.
Instant localize_strict(std::chrono::local_time<std::chrono::milliseconds> wall,
                        std::string_view zone);

// Moves by absolute time. The result keeps dt's zone; wall fields may shift across DST.
ZonedInstant add_elapsed(const ZonedInstant& dt, std::chrono::milliseconds delta);

// Moves by calendar days: keeps the wall-clock time and the zone. If the
// resulting wall time is ambiguous, the earlier occurrence is used.
ZonedInstant add_calendar_days(const ZonedInstant& dt, std::chrono::days n);

// Signed absolute difference end - start.
std::chrono::milliseconds elapsed_between(Instant start, Instant end);

}  // namespace cppref::chrono_time
