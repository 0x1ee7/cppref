// Shared vocabulary for the chrono exercise.
//
// Two kinds of "point in time" appear everywhere:
//   * Instant: an absolute UTC-based instant at millisecond resolution. It has no zone.
//   * ZonedInstant: an instant plus the IANA zone it is displayed in.
// Compare, subtract, and merge instants via their sys_time value, never via local (wall) fields.
#pragma once

#include <chrono>
#include <stdexcept>

namespace cppref::chrono_time {

using Instant = std::chrono::sys_time<std::chrono::milliseconds>;
using ZonedInstant = std::chrono::zoned_time<std::chrono::milliseconds>;

// Thrown when an IANA zone name is not present in the time zone database.
class UnknownZoneError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

}  // namespace cppref::chrono_time
