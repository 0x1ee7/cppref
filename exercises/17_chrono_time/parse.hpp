// Part 2: heterogeneous timestamp parsing into aware UTC Instants.
#pragma once

#include "common.hpp"

#include <cstdint>
#include <string_view>

namespace cppref::chrono_time {

// Accepts, after trimming leading/trailing ASCII whitespace:
//   * a numeric string: Unix epoch, seconds or milliseconds (see from_epoch_integer),
//     with an optional fraction ("1700000000.5");
//   * RFC 2822 starting with a weekday and comma ("Sun, 29 Mar 2026 12:00:00 +0200");
//     the "-0000" offset (unknown) is rejected;
//   * ISO 8601 / RFC 3339 with mandatory offset: "Z" or "+HH:MM", 'T' or space
//     separator, optional fractional seconds.
// Anything else, including naive or date-only ISO input and invalid calendar dates
// such as 2026-02-30, throws std::invalid_argument.
// Years outside 0001..9999 (UTC) throw std::out_of_range.
Instant parse_timestamp(std::string_view raw);

// Magnitude >= 100'000'000'000 means milliseconds, otherwise seconds.
Instant from_epoch_integer(std::int64_t value);

// Same magnitude rule as above, with fractional values rounded to the nearest
// millisecond (ties away from zero). NaN and infinities throw std::invalid_argument.
Instant from_epoch_float(double value);

}  // namespace cppref::chrono_time
