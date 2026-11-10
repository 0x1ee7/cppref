#include "slots.hpp"

#include <stdexcept>

namespace cppref::chrono_time {

TimeSlot::TimeSlot(ZonedInstant, ZonedInstant) {
    throw std::logic_error("not implemented: TimeSlot");
}

const ZonedInstant& TimeSlot::start() const noexcept {
    throw std::logic_error("not implemented: TimeSlot::start");
}

const ZonedInstant& TimeSlot::end() const noexcept {
    throw std::logic_error("not implemented: TimeSlot::end");
}

bool overlaps(const TimeSlot&, const TimeSlot&) noexcept {
    throw std::logic_error("not implemented: overlaps");
}

std::vector<UtcSlot> merge_slots(std::span<const TimeSlot>) {
    throw std::logic_error("not implemented: merge_slots");
}

std::chrono::milliseconds total_booked(std::span<const TimeSlot>) {
    throw std::logic_error("not implemented: total_booked");
}

}  // namespace cppref::chrono_time
