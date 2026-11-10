#include "zones.hpp"

#include <stdexcept>

namespace cppref::chrono_time {

const std::chrono::time_zone* find_zone(std::string_view) {
    throw std::logic_error("not implemented: find_zone");
}

ZonedInstant in_zone(Instant, std::string_view) {
    throw std::logic_error("not implemented: in_zone");
}

std::string format_in_zone(Instant, std::string_view) {
    throw std::logic_error("not implemented: format_in_zone");
}

Instant localize(std::chrono::local_time<std::chrono::milliseconds>, std::string_view,
                 std::chrono::choose) {
    throw std::logic_error("not implemented: localize");
}

Instant localize_strict(std::chrono::local_time<std::chrono::milliseconds>, std::string_view) {
    throw std::logic_error("not implemented: localize_strict");
}

ZonedInstant add_elapsed(const ZonedInstant&, std::chrono::milliseconds) {
    throw std::logic_error("not implemented: add_elapsed");
}

ZonedInstant add_calendar_days(const ZonedInstant&, std::chrono::days) {
    throw std::logic_error("not implemented: add_calendar_days");
}

std::chrono::milliseconds elapsed_between(Instant, Instant) {
    throw std::logic_error("not implemented: elapsed_between");
}

}  // namespace cppref::chrono_time
