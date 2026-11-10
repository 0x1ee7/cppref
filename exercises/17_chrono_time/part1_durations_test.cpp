// Part 1: durations, truncation vs floor vs round, epoch conversions.
#include "durations.hpp"
#include "test_support_test.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>

namespace {

using namespace cppref::chrono_time;
using namespace std::chrono_literals;
using cppref::chrono_time::fixture::utc;

TEST(Part1Durations, FloorAndTruncateDifferForNegativeValues) {
    // -1.5 s: floor goes toward -inf, truncate goes toward zero.
    EXPECT_EQ(floor_seconds(-1500ms), -2s);
    EXPECT_EQ(truncate_seconds(-1500ms), -1s);
    EXPECT_EQ(floor_seconds(1999ms), 1s);
    EXPECT_EQ(truncate_seconds(1999ms), 1s);
}

TEST(Part1Durations, RoundUsesTiesToEven) {
    EXPECT_EQ(round_seconds(2499ms), 2s);
    EXPECT_EQ(round_seconds(2500ms), 2s);  // tie: to even
    EXPECT_EQ(round_seconds(3500ms), 4s);  // tie: to even
    EXPECT_EQ(round_seconds(-1500ms), -2s);
}

TEST(Part1Durations, EpochMillisecondsFloorSubMillisecondTime) {
    const auto tp = std::chrono::system_clock::time_point{std::chrono::microseconds{1'700'000'000'123'456}};
    EXPECT_EQ(to_epoch_ms(tp), 1'700'000'000'123);
}

TEST(Part1Durations, EpochMillisecondsFloorBeforeTheEpoch) {
    // -1.5 ms floors to -2 ms; duration_cast would give -1.
    const auto tp = std::chrono::system_clock::time_point{std::chrono::microseconds{-1500}};
    EXPECT_EQ(to_epoch_ms(tp), -2);
    EXPECT_EQ(to_epoch_ms(std::chrono::system_clock::time_point{}), 0);
}

TEST(Part1Durations, MillisecondInstantFloorsToThePast) {
    const auto tp = std::chrono::system_clock::time_point{std::chrono::microseconds{-1}};
    EXPECT_EQ(to_millisecond_instant(tp), utc(1969, 12, 31, 23, 59, 59) + 999ms);
    EXPECT_EQ(to_millisecond_instant(tp).time_since_epoch(), -1ms);
    EXPECT_EQ(to_millisecond_instant(tp), utc(1970, 1, 1) - 1ms);
}

}  // namespace
