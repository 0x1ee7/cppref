#include "ingest.hpp"

#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

namespace {

using cppref::stl::Reading;
using cppref::stl::drop_invalid;
using cppref::stl::hottest_sensor;
using cppref::stl::select_sensor;
using cppref::stl::split_fields;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

std::vector<Reading> sample() {
    return {
        {"boiler-1", 61.5, 1000},
        {"boiler-2", kNaN, 1001},  // failed read
        {"boiler-1", 62.0, -1},    // device clock never synchronised
        {"pump-1", 3.25, 1003},
        {"boiler-1", 60.0, 1004},
    };
}

}  // namespace

TEST(Part1Ingest, DropInvalidRemovesNaNAndUnsyncedTimestampsInPlace) {
    auto readings = sample();

    EXPECT_EQ(drop_invalid(readings), 2u);

    ASSERT_EQ(readings.size(), 3u);
    EXPECT_EQ(readings[0], (Reading{"boiler-1", 61.5, 1000}));
    EXPECT_EQ(readings[1], (Reading{"pump-1", 3.25, 1003}));
    EXPECT_EQ(readings[2], (Reading{"boiler-1", 60.0, 1004}));
}

TEST(Part1Ingest, DropInvalidIsNoOpOnEmptyAndCleanInput) {
    std::vector<Reading> empty;
    EXPECT_EQ(drop_invalid(empty), 0u);
    EXPECT_TRUE(empty.empty());

    auto clean = std::vector<Reading>{{"a", 1.0, 0}, {"b", 2.0, 1}};
    const auto before = clean;
    EXPECT_EQ(drop_invalid(clean), 0u);
    EXPECT_EQ(clean, before);
}

TEST(Part1Ingest, SelectSensorKeepsMatchesInOrderWithCapacityForThem) {
    const auto readings = sample();

    const auto boiler = select_sensor(readings, "boiler-1");
    ASSERT_EQ(boiler.size(), 3u);
    EXPECT_EQ(boiler[0].ts_ms, 1000);
    EXPECT_EQ(boiler[1].ts_ms, -1);
    EXPECT_EQ(boiler[2].ts_ms, 1004);
    EXPECT_GE(boiler.capacity(), boiler.size());

    EXPECT_TRUE(select_sensor(readings, "no-such-sensor").empty());
}

TEST(Part1Ingest, SplitFieldsKeepsEmptyFields) {
    EXPECT_EQ(split_fields("a,,b,", ','), (std::vector<std::string_view>{"a", "", "b", ""}));
    EXPECT_EQ(split_fields("", ','), (std::vector<std::string_view>{""}));
    EXPECT_EQ(split_fields("solo", ','), (std::vector<std::string_view>{"solo"}));
}

TEST(Part1Ingest, SplitFieldsViewsAliasTheInputBuffer) {
    const std::string line = "sensor-7;61.5;1000";

    const auto parts = split_fields(line, ';');

    ASSERT_EQ(parts.size(), 3u);
    for (const auto part : parts) {
        EXPECT_GE(part.data(), line.data());
        EXPECT_LE(part.data() + part.size(), line.data() + line.size());
    }
}

TEST(Part1Ingest, HottestSensorIgnoresNaNAndBreaksTiesByPosition) {
    const std::vector<Reading> readings = {
        {"a", kNaN, 0},
        {"b", 9.0, 1},
        {"c", 9.0, 2},
        {"d", 1.0, 3},
    };

    const auto hot = hottest_sensor(readings);

    ASSERT_TRUE(hot.has_value());
    EXPECT_EQ(*hot, "b");
}

TEST(Part1Ingest, HottestSensorEmptyOrAllNaNIsNullopt) {
    EXPECT_EQ(hottest_sensor(std::span<const Reading>{}), std::nullopt);
    const std::vector<Reading> all_nan = {{"a", kNaN, 0}, {"b", kNaN, 1}};
    EXPECT_EQ(hottest_sensor(all_nan), std::nullopt);
}

TEST(Part1Ingest, HottestSensorViewAliasesReadingStorage) {
    const auto readings = sample();

    const auto hot = hottest_sensor(readings);

    ASSERT_TRUE(hot.has_value());
    EXPECT_EQ(hot->data(), readings[2].sensor.data());  // boiler-1 @ 62.0 is the maximum
}
