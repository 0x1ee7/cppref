// Part 6: time slots across zones, overlap, merge, and booked totals.
#include "slots.hpp"
#include "test_support_test.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

namespace {

using namespace cppref::chrono_time;
using namespace std::chrono_literals;
using cppref::chrono_time::fixture::utc;
using cppref::chrono_time::fixture::zoned;

TEST(Part6Slots, InvertedOrEmptySlotRejected) {
    const auto t = utc(2026, 3, 30, 9);
    EXPECT_THROW(TimeSlot(zoned("UTC", t), zoned("UTC", t)), std::invalid_argument);
    EXPECT_THROW(TimeSlot(zoned("UTC", t + 1h), zoned("UTC", t)), std::invalid_argument);
}

TEST(Part6Slots, CrossZoneBoundsAreAccepted) {
    // Tokyo 09:00 JST = 00:00Z; Berlin 03:00 CEST = 01:00Z. A one-hour slot across zones.
    const TimeSlot slot{zoned("Asia/Tokyo", utc(2026, 3, 30, 0)), zoned("Europe/Berlin", utc(2026, 3, 30, 1))};
    EXPECT_EQ(slot.start().get_sys_time(), utc(2026, 3, 30, 0));
    EXPECT_EQ(slot.end().get_sys_time(), utc(2026, 3, 30, 1));
}

TEST(Part6Slots, OverlapPartialContainedAndDisjoint) {
    const TimeSlot a{zoned("UTC", utc(2026, 3, 30, 9)), zoned("UTC", utc(2026, 3, 30, 11))};
    const TimeSlot partial{zoned("UTC", utc(2026, 3, 30, 10)), zoned("UTC", utc(2026, 3, 30, 12))};
    const TimeSlot inner{zoned("UTC", utc(2026, 3, 30, 9, 30)), zoned("UTC", utc(2026, 3, 30, 10))};
    const TimeSlot later{zoned("UTC", utc(2026, 3, 30, 12)), zoned("UTC", utc(2026, 3, 30, 13))};
    EXPECT_TRUE(overlaps(a, partial));
    EXPECT_TRUE(overlaps(partial, a));
    EXPECT_TRUE(overlaps(a, inner));
    EXPECT_FALSE(overlaps(a, later));
}

TEST(Part6Slots, TouchingSlotsDoNotOverlapAcrossZones) {
    // Tokyo end (00:00Z) equals Berlin start (00:00Z): touching, not overlapping.
    const TimeSlot tokyo{zoned("Asia/Tokyo", utc(2026, 3, 29, 23)), zoned("Asia/Tokyo", utc(2026, 3, 30, 0))};
    const TimeSlot berlin{zoned("Europe/Berlin", utc(2026, 3, 30, 0)), zoned("Europe/Berlin", utc(2026, 3, 30, 2))};
    EXPECT_FALSE(overlaps(tokyo, berlin));
}

TEST(Part6Slots, MergeSortsAndCombinesOverlappingAndTouching) {
    const std::vector<TimeSlot> slots{
        TimeSlot{zoned("UTC", utc(2026, 3, 30, 14)), zoned("UTC", utc(2026, 3, 30, 15))},
        TimeSlot{zoned("UTC", utc(2026, 3, 30, 9)), zoned("UTC", utc(2026, 3, 30, 10))},
        TimeSlot{zoned("UTC", utc(2026, 3, 30, 10)), zoned("UTC", utc(2026, 3, 30, 11))},
    };
    const auto merged = merge_slots(slots);
    ASSERT_EQ(merged.size(), 2u);
    EXPECT_EQ(merged[0], (UtcSlot{utc(2026, 3, 30, 9), utc(2026, 3, 30, 11)}));
    EXPECT_EQ(merged[1], (UtcSlot{utc(2026, 3, 30, 14), utc(2026, 3, 30, 15)}));
}

TEST(Part6Slots, MergeKeepsLargestEndWhenContained) {
    const std::vector<TimeSlot> slots{
        TimeSlot{zoned("UTC", utc(2026, 3, 30, 9)), zoned("UTC", utc(2026, 3, 30, 12))},
        TimeSlot{zoned("UTC", utc(2026, 3, 30, 10)), zoned("UTC", utc(2026, 3, 30, 11))},
    };
    const auto merged = merge_slots(slots);
    ASSERT_EQ(merged.size(), 1u);
    EXPECT_EQ(merged[0].end, utc(2026, 3, 30, 12));
}

TEST(Part6Slots, MergeNormalisesToUtc) {
    const std::vector<TimeSlot> slots{
        TimeSlot{zoned("Asia/Tokyo", utc(2026, 3, 30, 0)), zoned("Asia/Tokyo", utc(2026, 3, 30, 1))},
    };
    const auto merged = merge_slots(slots);
    ASSERT_EQ(merged.size(), 1u);
    EXPECT_EQ(merged[0], (UtcSlot{utc(2026, 3, 30, 0), utc(2026, 3, 30, 1)}));
}

TEST(Part6Slots, TotalBookedCountsOverlapOnce) {
    const std::vector<TimeSlot> slots{
        TimeSlot{zoned("UTC", utc(2026, 3, 30, 9)), zoned("UTC", utc(2026, 3, 30, 11))},
        TimeSlot{zoned("UTC", utc(2026, 3, 30, 10)), zoned("UTC", utc(2026, 3, 30, 12))},
    };
    EXPECT_EQ(total_booked(slots), 3h);
}

TEST(Part6Slots, TotalBookedAcrossZonesIsAbsolute) {
    // Tokyo 09:00-10:00 JST = 00:00-01:00Z; Berlin 02:30-03:30 CEST = 00:30-01:30Z.
    // Union is 00:00-01:30Z = 90 minutes.
    const std::vector<TimeSlot> slots{
        TimeSlot{zoned("Asia/Tokyo", utc(2026, 3, 30, 0)), zoned("Asia/Tokyo", utc(2026, 3, 30, 1))},
        TimeSlot{zoned("Europe/Berlin", utc(2026, 3, 30, 0, 30)), zoned("Europe/Berlin", utc(2026, 3, 30, 1, 30))},
    };
    EXPECT_EQ(total_booked(slots), 90min);
}

TEST(Part6Slots, TotalBookedSameZoneDstSpanIsElapsedTime) {
    // Berlin 28 March 12:00 CET to 29 March 12:00 CEST spans 23 absolute hours.
    const std::vector<TimeSlot> slots{
        TimeSlot{zoned("Europe/Berlin", utc(2026, 3, 28, 11)), zoned("Europe/Berlin", utc(2026, 3, 29, 10))},
    };
    EXPECT_EQ(total_booked(slots), 23h);
}

TEST(Part6Slots, EmptyInputsAreZeroAndEmpty) {
    const std::vector<TimeSlot> none;
    EXPECT_EQ(total_booked(none), 0ms);
    EXPECT_TRUE(merge_slots(none).empty());
}

}  // namespace
