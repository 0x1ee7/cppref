// Part 6: booking slots across time zones.
#pragma once

#include "common.hpp"

#include <chrono>
#include <span>
#include <vector>

namespace cppref::chrono_time {

// Half-open interval [start, end). Each bound keeps its own zone for display.
class TimeSlot {
public:
    // Throws std::invalid_argument unless end is strictly after start (compared as instants).
    TimeSlot(ZonedInstant start, ZonedInstant end);

    const ZonedInstant& start() const noexcept;
    const ZonedInstant& end() const noexcept;

private:
    // Engineer: store start and end here.
};

// A normalised merged interval, in UTC.
struct UtcSlot {
    Instant start;
    Instant end;

    friend bool operator==(const UtcSlot&, const UtcSlot&) = default;
};

// True when the two half-open intervals share an instant. Touching slots do not overlap.
bool overlaps(const TimeSlot& a, const TimeSlot& b) noexcept;

// Sorted, merged, UTC. Overlapping or touching slots are combined.
std::vector<UtcSlot> merge_slots(std::span<const TimeSlot> slots);

// Total covered time after merging: overlapped time is counted once.
std::chrono::milliseconds total_booked(std::span<const TimeSlot> slots);

}  // namespace cppref::chrono_time
