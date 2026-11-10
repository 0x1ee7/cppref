#include "slots.hpp"

#include <algorithm>
#include <stdexcept>

namespace cppref::chrono_time {

using namespace std::chrono;

TimeSlot::TimeSlot(ZonedInstant start, ZonedInstant end)
    : start_(std::move(start)), end_(std::move(end)) {
    if (!(end_.get_sys_time() > start_.get_sys_time())) {
        throw std::invalid_argument("slot end must be strictly after start");
    }
}

const ZonedInstant& TimeSlot::start() const noexcept {
    return start_;
}

const ZonedInstant& TimeSlot::end() const noexcept {
    return end_;
}

bool overlaps(const TimeSlot& a, const TimeSlot& b) noexcept {
    return a.start().get_sys_time() < b.end().get_sys_time() &&
           b.start().get_sys_time() < a.end().get_sys_time();
}

std::vector<UtcSlot> merge_slots(std::span<const TimeSlot> slots) {
    std::vector<UtcSlot> sorted;
    sorted.reserve(slots.size());
    for (const auto& s : slots) {
        sorted.push_back(UtcSlot{s.start().get_sys_time(), s.end().get_sys_time()});
    }
    std::ranges::sort(sorted, {}, &UtcSlot::start);

    std::vector<UtcSlot> merged;
    for (const auto& cur : sorted) {
        if (!merged.empty() && cur.start <= merged.back().end) {
            merged.back().end = std::max(merged.back().end, cur.end);
        } else {
            merged.push_back(cur);
        }
    }
    return merged;
}

milliseconds total_booked(std::span<const TimeSlot> slots) {
    milliseconds total{0};
    for (const auto& m : merge_slots(slots)) {
        total += m.end - m.start;
    }
    return total;
}

}  // namespace cppref::chrono_time
