// Part 3: time zones, DST gaps and overlaps, wall-clock vs elapsed arithmetic.
#include "zones.hpp"
#include "test_support_test.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>

namespace {

using namespace cppref::chrono_time;
using namespace std::chrono_literals;
using cppref::chrono_time::fixture::utc;
using cppref::chrono_time::fixture::wall;
using cppref::chrono_time::fixture::zoned;

TEST(Part3Zones, FindZoneKnownAndUnknown) {
    EXPECT_EQ(find_zone("Europe/Berlin")->name(), "Europe/Berlin");
    EXPECT_THROW(find_zone("Mars/Olympus_Mons"), UnknownZoneError);
}

TEST(Part3Zones, InZoneShowsWallTimeWithDst) {
    // Berlin's DST began at 01:00 UTC on 29 March 2026 (02:00 CET jumps to 03:00 CEST), so 12:00 UTC is 14:00 CEST.
    const auto zt = in_zone(utc(2026, 3, 29, 12), "Europe/Berlin");
    EXPECT_EQ(zt.get_local_time(), wall(2026, 3, 29, 14));
    EXPECT_EQ(in_zone(utc(2026, 1, 15, 12), "Europe/Berlin").get_local_time(), wall(2026, 1, 15, 13));
}

TEST(Part3Zones, FormatInZone) {
    EXPECT_EQ(format_in_zone(utc(2026, 3, 29, 12), "Europe/Berlin"), "2026-03-29 14:00 CEST");
    EXPECT_EQ(format_in_zone(utc(2026, 1, 15, 12), "Europe/Berlin"), "2026-01-15 13:00 CET");
    EXPECT_EQ(format_in_zone(utc(2026, 3, 29, 12), "UTC"), "2026-03-29 12:00 UTC");
}

TEST(Part3Zones, LocalizeAmbiguousWallTimeUsesChoice) {
    // 25 Oct 2026, 02:30 happens twice in Berlin: CEST (00:30Z) then CET (01:30Z).
    EXPECT_EQ(localize(wall(2026, 10, 25, 2, 30), "Europe/Berlin", std::chrono::choose::earliest),
              utc(2026, 10, 25, 0, 30));
    EXPECT_EQ(localize(wall(2026, 10, 25, 2, 30), "Europe/Berlin", std::chrono::choose::latest),
              utc(2026, 10, 25, 1, 30));
}

TEST(Part3Zones, LocalizeStrictThrowsOnAmbiguousAndNonexistent) {
    EXPECT_THROW(localize_strict(wall(2026, 10, 25, 2, 30), "Europe/Berlin"),
                 std::chrono::ambiguous_local_time);
    EXPECT_THROW(localize_strict(wall(2026, 3, 29, 2, 30), "Europe/Berlin"),
                 std::chrono::nonexistent_local_time);
    EXPECT_EQ(localize_strict(wall(2026, 6, 1, 12), "Europe/Berlin"), utc(2026, 6, 1, 10));
}

TEST(Part3Zones, AddElapsedIsAbsoluteAcrossDst) {
    // 12:00 CET on 28 March = 11:00Z. Plus 24 absolute hours = 11:00Z on 29 March = 13:00 CEST.
    const auto start = zoned("Europe/Berlin", utc(2026, 3, 28, 11));
    const auto moved = add_elapsed(start, 24h);
    EXPECT_EQ(moved.get_sys_time(), utc(2026, 3, 29, 11));
    EXPECT_EQ(moved.get_local_time(), wall(2026, 3, 29, 13));
    EXPECT_EQ(moved.get_time_zone()->name(), "Europe/Berlin");
}

TEST(Part3Zones, AddCalendarDaysKeepsWallClockAcrossDst) {
    // Same start, but one calendar day later: 12:00 local on 29 March = 10:00Z.
    const auto start = zoned("Europe/Berlin", utc(2026, 3, 28, 11));
    const auto moved = add_calendar_days(start, std::chrono::days{1});
    EXPECT_EQ(moved.get_local_time(), wall(2026, 3, 29, 12));
    EXPECT_EQ(moved.get_sys_time(), utc(2026, 3, 29, 10));
    EXPECT_EQ(moved.get_time_zone()->name(), "Europe/Berlin");
}

TEST(Part3Zones, ElapsedBetweenIsSigned) {
    EXPECT_EQ(elapsed_between(utc(2026, 3, 29, 10), utc(2026, 3, 29, 9)), -1h);
    EXPECT_EQ(elapsed_between(utc(2026, 3, 29, 9), utc(2026, 3, 29, 10, 0, 0) + 500ms),
              1h + 500ms);
}

}  // namespace
