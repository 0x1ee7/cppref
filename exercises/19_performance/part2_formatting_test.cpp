// Part 2 tests: single-allocation string building and fixed-buffer formatting.
#include "formatting.hpp"
#include "perf_support_test.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using cppref::performance::AccessEntry;
using cppref::performance::format_access_line;
using cppref::performance::format_access_line_into;
using cppref::performance::join_csv;
using cppref::performance::testing::AllocScope;
using cppref::performance::testing::container_overhead;

AccessEntry sample_entry() { return AccessEntry{"GET", "/v1/items", 200, 1532}; }

TEST(Part2Csv, JoinsFieldsWithComma) {
    const std::vector<std::string_view> fields{"a", "b", "c"};
    EXPECT_EQ(join_csv(fields), "a,b,c");
}

TEST(Part2Csv, SingleFieldHasNoSeparator) {
    const std::vector<std::string_view> fields{"solo"};
    EXPECT_EQ(join_csv(fields), "solo");
}

TEST(Part2Csv, EmptyFieldsAreKept) {
    const std::vector<std::string_view> fields{"a", "", "c"};
    EXPECT_EQ(join_csv(fields), "a,,c");
}

TEST(Part2Csv, EmptySpanYieldsEmptyString) {
    const std::vector<std::string_view> fields;
    EXPECT_EQ(join_csv(fields), "");
}

TEST(Part2Csv, RejectsCommaInsideAField) {
    const std::vector<std::string_view> fields{"a,b"};
    EXPECT_THROW(join_csv(fields), std::invalid_argument);
}

TEST(Part2Csv, RejectsNewlineInsideAField) {
    const std::vector<std::string_view> fields{"a\nb"};
    EXPECT_THROW(join_csv(fields), std::invalid_argument);
}

TEST(Part2Format, CanonicalText) {
    EXPECT_EQ(format_access_line(sample_entry()), "GET /v1/items 200 1532us");
}

TEST(Part2Format, ZeroLatencyStillCarriesUnit) {
    EXPECT_EQ(format_access_line(AccessEntry{"HEAD", "/", 204, 0}), "HEAD / 204 0us");
}

TEST(Part2Format, LargestLatency) {
    EXPECT_EQ(format_access_line(AccessEntry{"GET", "/", 200, 18446744073709551615ull}),
              "GET / 200 18446744073709551615us");
}

TEST(Part2Into, WritesIntoAnExactlySizedBuffer) {
    const std::string expected = "GET /v1/items 200 1532us";
    std::vector<char> buffer(expected.size());
    auto r = format_access_line_into(buffer, sample_entry());
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, expected);
}

TEST(Part2Into, TooSmallBufferReportsRequiredSize) {
    const std::string expected = "GET /v1/items 200 1532us";
    std::vector<char> buffer(5);
    auto r = format_access_line_into(buffer, sample_entry());
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), expected.size());
}

TEST(Part2Alloc, JoinCsvAllocatesAtMostOnce) {
    std::vector<std::string> storage(200, "field");
    std::vector<std::string_view> views(storage.begin(), storage.end());
    const std::size_t overhead = container_overhead<std::string>();
    AllocScope scope;
    const std::string out = join_csv(views);
    const std::size_t allocs = scope.allocations() - overhead;
    EXPECT_EQ(out.size(), 200u * 5u + 199u);
    EXPECT_LE(allocs, 1u);
}

TEST(Part2Alloc, JoinCsvOfEmptySpanAllocatesNothing) {
    const std::vector<std::string_view> fields;
    const std::size_t overhead = container_overhead<std::string>();
    AllocScope scope;
    const std::string out = join_csv(fields);
    const std::size_t allocs = scope.allocations() - overhead;
    EXPECT_EQ(out, "");
    EXPECT_EQ(allocs, 0u);
}

TEST(Part2Alloc, FormatAccessLineAllocatesAtMostOnce) {
    const std::size_t overhead = container_overhead<std::string>();
    AllocScope scope;
    const std::string out = format_access_line(sample_entry());
    const std::size_t allocs = scope.allocations() - overhead;
    EXPECT_EQ(out, "GET /v1/items 200 1532us");
    EXPECT_LE(allocs, 1u);
}

TEST(Part2Alloc, FormatAccessLineIntoAllocatesNothing) {
    std::array<char, 64> buffer{};
    AllocScope scope;
    auto r = format_access_line_into(buffer, sample_entry());
    const std::size_t allocs = scope.allocations();
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, "GET /v1/items 200 1532us");
    EXPECT_EQ(allocs, 0u);
}

}  // namespace
