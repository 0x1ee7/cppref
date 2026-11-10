#include "durations.hpp"

namespace cppref::chrono_time {

using std::chrono::milliseconds;
using std::chrono::seconds;

std::int64_t to_epoch_ms(std::chrono::system_clock::time_point tp) {
    return std::chrono::floor<milliseconds>(tp.time_since_epoch()).count();
}

Instant to_millisecond_instant(std::chrono::system_clock::time_point tp) {
    return Instant{std::chrono::floor<milliseconds>(tp.time_since_epoch())};
}

seconds floor_seconds(milliseconds d) {
    return std::chrono::floor<seconds>(d);
}

seconds truncate_seconds(milliseconds d) {
    return std::chrono::duration_cast<seconds>(d);
}

seconds round_seconds(milliseconds d) {
    return std::chrono::round<seconds>(d);
}

}  // namespace cppref::chrono_time
