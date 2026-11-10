// Part 5: recurring meetings with an explicit DST policy.
#pragma once

#include "common.hpp"

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

namespace cppref::chrono_time {

// Policy for a wall time that does not exist (spring-forward gap).
enum class Gap {
    skip,           // drop the occurrence; it does not count toward `count`
    shift_forward,  // the instant the clock jumps to: 02:30 becomes 03:30 CEST
};

// Policy for a wall time that occurs twice (fall-back overlap).
enum class Overlap {
    earlier,  // first occurrence (still on the pre-transition offset)
    later,    // second occurrence
    skip,     // drop the occurrence
};

struct WeeklyRule {
    std::chrono::year_month_day start;   // first date eligible (on or after)
    std::chrono::weekday day;            // Monday == std::chrono::Monday
    std::chrono::minutes at;             // wall time since local midnight, in [0h, 24h)
    std::string zone;                    // IANA name, e.g. "Europe/Berlin"
    std::size_t count;                   // number of occurrences to return
    Gap gap = Gap::shift_forward;
    Overlap overlap = Overlap::earlier;
};

// The next `count` occurrences at or after rule.start, ascending, as UTC instants.
// Throws std::invalid_argument for an invalid start date or `at` outside [0h, 24h),
// and UnknownZoneError for an unknown zone.
std::vector<Instant> weekly_occurrences(const WeeklyRule& rule);

}  // namespace cppref::chrono_time
