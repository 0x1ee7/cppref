// Part 1: durations, truncation, and epoch conversions.
//
// Rounding policy matters: duration_cast truncates toward zero, which is wrong
// for values before 1970. floor/round/truncate are distinct operations.
#pragma once

#include "common.hpp"

#include <cstdint>

namespace cppref::chrono_time {

// Whole milliseconds since 1970-01-01T00:00Z, rounded toward the past (floor).
// -1 us becomes -1 ms, not 0 ms.
std::int64_t to_epoch_ms(std::chrono::system_clock::time_point tp);

// The same floor rule, returned as an Instant.
Instant to_millisecond_instant(std::chrono::system_clock::time_point tp);

// Toward negative infinity: floor_seconds(-1500 ms) == -2 s.
std::chrono::seconds floor_seconds(std::chrono::milliseconds d);

// Toward zero (duration_cast): truncate_seconds(-1500 ms) == -1 s.
std::chrono::seconds truncate_seconds(std::chrono::milliseconds d);

// Nearest whole second, ties to even (std::chrono::round):
// round_seconds(2500 ms) == 2 s, round_seconds(3500 ms) == 4 s.
std::chrono::seconds round_seconds(std::chrono::milliseconds d);

}  // namespace cppref::chrono_time
