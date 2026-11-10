#include "durations.hpp"

#include <stdexcept>

namespace cppref::chrono_time {

std::int64_t to_epoch_ms(std::chrono::system_clock::time_point) {
    throw std::logic_error("not implemented: to_epoch_ms");
}

Instant to_millisecond_instant(std::chrono::system_clock::time_point) {
    throw std::logic_error("not implemented: to_millisecond_instant");
}

std::chrono::seconds floor_seconds(std::chrono::milliseconds) {
    throw std::logic_error("not implemented: floor_seconds");
}

std::chrono::seconds truncate_seconds(std::chrono::milliseconds) {
    throw std::logic_error("not implemented: truncate_seconds");
}

std::chrono::seconds round_seconds(std::chrono::milliseconds) {
    throw std::logic_error("not implemented: round_seconds");
}

}  // namespace cppref::chrono_time
