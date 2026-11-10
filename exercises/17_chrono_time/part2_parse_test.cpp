// Part 2: heterogeneous timestamp parsing into aware UTC instants.
#include "parse.hpp"
#include "test_support_test.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

using namespace cppref::chrono_time;
using namespace std::chrono_literals;
using cppref::chrono_time::fixture::utc;

// 2023-11-14T22:13:20Z
const Instant kEpoch1700000000 = utc(2023, 11, 14, 22, 13, 20);

TEST(Part2Parse, IntegerEpochSecondsAndMilliseconds) {
    EXPECT_EQ(from_epoch_integer(1'700'000'000), kEpoch1700000000);
    EXPECT_EQ(from_epoch_integer(1'700'000'000'123), kEpoch1700000000 + 123ms);
    // Boundary: 1e11 is the first value read as milliseconds.
    EXPECT_EQ(from_epoch_integer(100'000'000'000), utc(1973, 3, 3, 9, 46, 40));
    EXPECT_EQ(from_epoch_integer(-86'400), utc(1969, 12, 31));
}

TEST(Part2Parse, FloatKeepsFractionAndRoundsToNearestMillisecond) {
    EXPECT_EQ(from_epoch_float(1'700'000'000.25), kEpoch1700000000 + 250ms);
    EXPECT_EQ(from_epoch_float(1'700'000'000'123.0), kEpoch1700000000 + 123ms);
    EXPECT_EQ(from_epoch_float(1'700'000'000.123), kEpoch1700000000 + 123ms);
}

TEST(Part2Parse, NonFiniteFloatsRejected) {
    EXPECT_THROW(from_epoch_float(std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
    EXPECT_THROW(from_epoch_float(std::numeric_limits<double>::infinity()), std::invalid_argument);
}

TEST(Part2Parse, OutOfRangeEpochThrows) {
    // 3e14 ms is roughly year 11477.
    EXPECT_THROW(from_epoch_integer(300'000'000'000'000), std::out_of_range);
}

TEST(Part2Parse, IsoWithZuluAndOffsetNormalisesToUtc) {
    EXPECT_EQ(parse_timestamp("2023-11-14T22:13:20Z"), kEpoch1700000000);
    EXPECT_EQ(parse_timestamp("2023-11-14T23:13:20+01:00"), kEpoch1700000000);
    EXPECT_EQ(parse_timestamp("2023-11-14 22:13:20.250Z"), kEpoch1700000000 + 250ms);
}

TEST(Part2Parse, NaiveDateOnlyAndInvalidCalendarDatesRejected) {
    EXPECT_THROW(parse_timestamp("2023-11-14T22:13:20"), std::invalid_argument);
    EXPECT_THROW(parse_timestamp("2023-11-14"), std::invalid_argument);
    EXPECT_THROW(parse_timestamp("2026-02-30T10:00:00Z"), std::invalid_argument);
    EXPECT_THROW(parse_timestamp("2023-11-14T22:13:20Zjunk"), std::invalid_argument);
}

TEST(Part2Parse, Rfc2822ParsedWithOffset) {
    EXPECT_EQ(parse_timestamp("Sun, 29 Mar 2026 12:00:00 +0200"), utc(2026, 3, 29, 10));
}

TEST(Part2Parse, Rfc2822UnknownOffsetRejected) {
    EXPECT_THROW(parse_timestamp("Sun, 29 Mar 2026 12:00:00 -0000"), std::invalid_argument);
}

TEST(Part2Parse, NumericStringsAreEpochValues) {
    EXPECT_EQ(parse_timestamp("1700000000"), kEpoch1700000000);
    EXPECT_EQ(parse_timestamp("1700000000123"), kEpoch1700000000 + 123ms);
    EXPECT_EQ(parse_timestamp("-86400"), utc(1969, 12, 31));
    EXPECT_EQ(parse_timestamp("1700000000.5"), kEpoch1700000000 + 500ms);
    EXPECT_EQ(parse_timestamp("  1700000000  "), kEpoch1700000000);  // surrounding whitespace ignored
}

TEST(Part2Parse, EmptyAndGarbageRejected) {
    EXPECT_THROW(parse_timestamp(""), std::invalid_argument);
    EXPECT_THROW(parse_timestamp("   "), std::invalid_argument);
    EXPECT_THROW(parse_timestamp("not a time"), std::invalid_argument);
}

}  // namespace
