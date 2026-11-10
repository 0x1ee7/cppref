// Part 5: DST-aware recurring meetings with explicit gap/overlap policies.
#include "meetings.hpp"
#include "test_support_test.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace {

using namespace cppref::chrono_time;
using namespace std::chrono;
using cppref::chrono_time::fixture::utc;

year_month_day ymd(int y, unsigned m, unsigned d) {
    return year_month_day{year{y}, month{m}, day{d}};
}

TEST(Part5Meetings, MondayNineAmBerlinIsStableOnWallClock) {
    // Winter (CET) gives 08:00Z; the wall clock stays 09:00 after the March switch.
    const auto occ = weekly_occurrences(WeeklyRule{
        .start = ymd(2026, 3, 23), .day = Monday, .at = minutes{9 * 60},
        .zone = "Europe/Berlin", .count = 3});
    ASSERT_EQ(occ.size(), 3u);
    EXPECT_EQ(occ[0], utc(2026, 3, 23, 8));
    EXPECT_EQ(occ[1], utc(2026, 3, 30, 7));
    EXPECT_EQ(occ[2], utc(2026, 4, 6, 7));
}

TEST(Part5Meetings, StartOnTuesdayRollsToNextMonday) {
    const auto occ = weekly_occurrences(WeeklyRule{
        .start = ymd(2026, 3, 3), .day = Monday, .at = minutes{9 * 60},
        .zone = "Europe/Berlin", .count = 1});
    ASSERT_EQ(occ.size(), 1u);
    EXPECT_EQ(occ[0], utc(2026, 3, 9, 8));
}

TEST(Part5Meetings, NonexistentLocalTimeShiftForwardByDefault) {
    // Sunday 29 March 2026 02:30 does not exist in Berlin; shift_forward gives 03:30 CEST.
    const auto occ = weekly_occurrences(WeeklyRule{
        .start = ymd(2026, 3, 22), .day = Sunday, .at = minutes{2 * 60 + 30},
        .zone = "Europe/Berlin", .count = 2});
    ASSERT_EQ(occ.size(), 2u);
    EXPECT_EQ(occ[0], utc(2026, 3, 22, 1, 30));
    EXPECT_EQ(occ[1], utc(2026, 3, 29, 1, 30));
}

TEST(Part5Meetings, NonexistentLocalTimeSkipPolicy) {
    const auto occ = weekly_occurrences(WeeklyRule{
        .start = ymd(2026, 3, 22), .day = Sunday, .at = minutes{2 * 60 + 30},
        .zone = "Europe/Berlin", .count = 2, .gap = Gap::skip});
    ASSERT_EQ(occ.size(), 2u);
    EXPECT_EQ(occ[0], utc(2026, 3, 22, 1, 30));
    EXPECT_EQ(occ[1], utc(2026, 4, 5, 0, 30));
}

TEST(Part5Meetings, AmbiguousLocalTimeEarlierByDefault) {
    const auto occ = weekly_occurrences(WeeklyRule{
        .start = ymd(2026, 10, 25), .day = Sunday, .at = minutes{2 * 60 + 30},
        .zone = "Europe/Berlin", .count = 1});
    ASSERT_EQ(occ.size(), 1u);
    EXPECT_EQ(occ[0], utc(2026, 10, 25, 0, 30));
}

TEST(Part5Meetings, AmbiguousLocalTimeLaterPolicy) {
    const auto occ = weekly_occurrences(WeeklyRule{
        .start = ymd(2026, 10, 25), .day = Sunday, .at = minutes{2 * 60 + 30},
        .zone = "Europe/Berlin", .count = 1, .overlap = Overlap::later});
    ASSERT_EQ(occ.size(), 1u);
    EXPECT_EQ(occ[0], utc(2026, 10, 25, 1, 30));
}

TEST(Part5Meetings, AmbiguousLocalTimeSkipPolicy) {
    const auto occ = weekly_occurrences(WeeklyRule{
        .start = ymd(2026, 10, 25), .day = Sunday, .at = minutes{2 * 60 + 30},
        .zone = "Europe/Berlin", .count = 2, .overlap = Overlap::skip});
    ASSERT_EQ(occ.size(), 2u);
    EXPECT_EQ(occ[0], utc(2026, 11, 1, 1, 30));
}

TEST(Part5Meetings, TokyoMondayNineAmIsMidnightUtc) {
    const auto occ = weekly_occurrences(WeeklyRule{
        .start = ymd(2026, 3, 2), .day = Monday, .at = minutes{9 * 60},
        .zone = "Asia/Tokyo", .count = 1});
    ASSERT_EQ(occ.size(), 1u);
    EXPECT_EQ(occ[0], utc(2026, 3, 2, 0));
}

TEST(Part5Meetings, ZeroCountIsEmpty) {
    EXPECT_TRUE(weekly_occurrences(WeeklyRule{
                    .start = ymd(2026, 3, 2), .day = Monday, .at = minutes{0},
                    .zone = "UTC", .count = 0})
                    .empty());
}

TEST(Part5Meetings, InvalidInputsRejected) {
    EXPECT_THROW(weekly_occurrences(WeeklyRule{.start = ymd(2026, 3, 2), .day = Monday,
                                               .at = hours{24}, .zone = "UTC", .count = 1}),
                 std::invalid_argument);
    EXPECT_THROW(weekly_occurrences(WeeklyRule{.start = year_month_day{}, .day = Monday,
                                               .at = minutes{0}, .zone = "UTC", .count = 1}),
                 std::invalid_argument);
    EXPECT_THROW(weekly_occurrences(WeeklyRule{.start = ymd(2026, 3, 2), .day = Monday,
                                               .at = minutes{0}, .zone = "Mars/Base", .count = 1}),
                 UnknownZoneError);
}

}  // namespace
