// Part 7a: humanized relative time with an injected "now".
#pragma once

#include "common.hpp"

#include <string>

namespace cppref::chrono_time {

// Uses absolute time |when - now|, never wall-clock fields. Rules:
//   under 45 s        -> "just now"
//   under 1 hour      -> minutes (at least 1): "1 minute ago", "in 59 minutes"
//   under 24 hours    -> hours: "3 hours ago", "in 1 hour"
//   under 30 days     -> days: "2 days ago", "in 2 days"
//   otherwise         -> "on YYYY-MM-DD", the date of `when` in its own zone
// Future instants use "in ...", past instants "... ago". Units are singular for 1.
std::string humanize(const ZonedInstant& when, const ZonedInstant& now);

}  // namespace cppref::chrono_time
