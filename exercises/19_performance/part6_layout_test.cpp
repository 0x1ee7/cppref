// Part 6 tests: data layout, columnar conversion and allocation-bounded aggregation.
#include "layout.hpp"
#include "perf_support_test.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

using cppref::performance::Sample;
using cppref::performance::SampleColumns;
using cppref::performance::SensorSummary;
using cppref::performance::summarize_by_sensor;
using cppref::performance::to_columns;
using cppref::performance::testing::AllocScope;
using cppref::performance::testing::container_overhead;

Sample make_sample(std::uint32_t sensor, double value, std::int64_t timestamp_ns = 0) {
    Sample s{};
    s.kind = 1;
    s.flags = 0;
    s.value = value;
    s.timestamp_ns = timestamp_ns;
    s.sensor_id = sensor;
    return s;
}

// ---- Layout ------------------------------------------------------------------------------------

TEST(Part6Layout, SampleIsTwentyFourBytesAndEightByteAligned) {
    EXPECT_EQ(sizeof(Sample), 24u);
    EXPECT_EQ(alignof(Sample), 8u);
}

// ---- Columns -----------------------------------------------------------------------------------

TEST(Part6Columns, PreservesOrderAndDropsKindAndFlags) {
    const std::vector<Sample> samples{make_sample(4, 1.5, 100), make_sample(9, -2.0, 200),
                                      make_sample(4, 3.25, 300)};
    const SampleColumns cols = to_columns(samples);
    EXPECT_EQ(cols.timestamp_ns, (std::vector<std::int64_t>{100, 200, 300}));
    EXPECT_EQ(cols.value, (std::vector<double>{1.5, -2.0, 3.25}));
    EXPECT_EQ(cols.sensor_id, (std::vector<std::uint32_t>{4, 9, 4}));
}

TEST(Part6Columns, EmptyInputGivesEmptyColumns) {
    const SampleColumns cols = to_columns(std::vector<Sample>{});
    EXPECT_TRUE(cols.timestamp_ns.empty());
    EXPECT_TRUE(cols.value.empty());
    EXPECT_TRUE(cols.sensor_id.empty());
}

TEST(Part6Columns, LargeBatchUsesAtMostThreeAllocations) {
    std::vector<Sample> samples;
    samples.reserve(10000);
    for (std::uint32_t i = 0; i < 10000; ++i) samples.push_back(make_sample(i % 5, i));
    const std::size_t overhead = container_overhead<SampleColumns>();
    AllocScope scope;
    const SampleColumns cols = to_columns(samples);
    const std::size_t allocs = scope.allocations() - overhead;
    EXPECT_EQ(cols.value.size(), 10000u);
    EXPECT_LE(allocs, 3u);
}

// ---- Aggregation -------------------------------------------------------------------------------

TEST(Part6Summary, GroupsBySensorSortedAscending) {
    const std::vector<Sample> samples{make_sample(7, 1.0), make_sample(3, 2.0), make_sample(7, 3.0),
                                      make_sample(9, 4.0), make_sample(3, 6.0)};
    const std::vector<SensorSummary> summary = summarize_by_sensor(to_columns(samples));
    ASSERT_EQ(summary.size(), 3u);

    EXPECT_EQ(summary[0].sensor_id, 3u);
    EXPECT_EQ(summary[0].count, 2u);
    EXPECT_DOUBLE_EQ(summary[0].mean, 4.0);
    EXPECT_DOUBLE_EQ(summary[0].max, 6.0);

    EXPECT_EQ(summary[1].sensor_id, 7u);
    EXPECT_EQ(summary[1].count, 2u);
    EXPECT_DOUBLE_EQ(summary[1].mean, 2.0);
    EXPECT_DOUBLE_EQ(summary[1].max, 3.0);

    EXPECT_EQ(summary[2].sensor_id, 9u);
    EXPECT_EQ(summary[2].count, 1u);
    EXPECT_DOUBLE_EQ(summary[2].mean, 4.0);
    EXPECT_DOUBLE_EQ(summary[2].max, 4.0);
}

TEST(Part6Summary, EmptyInputGivesNoSummaries) {
    EXPECT_TRUE(summarize_by_sensor(SampleColumns{}).empty());
}

TEST(Part6Summary, NegativeValuesKeepTheirMax) {
    const std::vector<Sample> samples{make_sample(1, -5.0), make_sample(1, -2.0),
                                      make_sample(1, -9.0)};
    const std::vector<SensorSummary> summary = summarize_by_sensor(to_columns(samples));
    ASSERT_EQ(summary.size(), 1u);
    EXPECT_DOUBLE_EQ(summary[0].max, -2.0);
    EXPECT_DOUBLE_EQ(summary[0].mean, -16.0 / 3.0);
}

TEST(Part6Alloc, SummaryUsesAtMostThreeAllocations) {
    std::vector<Sample> samples;
    samples.reserve(10000);
    for (std::uint32_t i = 0; i < 10000; ++i) samples.push_back(make_sample(i % 5, i));
    const SampleColumns cols = to_columns(samples);
    const std::size_t overhead =
        container_overhead<std::vector<SensorSummary>>() + container_overhead<std::vector<std::size_t>>();
    AllocScope scope;
    const std::vector<SensorSummary> summary = summarize_by_sensor(cols);
    const std::size_t allocs = scope.allocations() - overhead;
    ASSERT_EQ(summary.size(), 5u);
    EXPECT_EQ(summary[0].count, 2000u);
    EXPECT_LE(allocs, 3u);
}

}  // namespace
