#include "meetings.hpp"

#include <stdexcept>

namespace cppref::chrono_time {

std::vector<Instant> weekly_occurrences(const WeeklyRule&) {
    throw std::logic_error("not implemented: weekly_occurrences");
}

}  // namespace cppref::chrono_time
