// Part 2: log record parsing, predicates, and range helpers.
#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <concepts>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include "log_records.hpp"

namespace ir = cppref::iterators_and_ranges;
namespace views = std::ranges::views;
using ir::Level;
using ir::LogRecord;

namespace {

// 2026-03-01T12:HH:MM:SS UTC as a sys_seconds.
std::chrono::sys_seconds ts(int h, int m, int s) {
    using namespace std::chrono;
    return sys_seconds{sys_days{year{2026} / March / 1}} + hours{h} + minutes{m} + seconds{s};
}

LogRecord make(std::chrono::sys_seconds t, Level level, std::string message) {
    return LogRecord{t, level, "web-1", std::move(message)};
}

}  // namespace

TEST(Part2Pipelines, ParsesValidLine) {
    auto r = ir::parse_line("2026-03-01T12:05:00Z WARNING web-1 disk at 91% on /var");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->ts, ts(12, 5, 0));
    EXPECT_EQ(r->level, Level::warning);
    EXPECT_EQ(r->server, "web-1");
    EXPECT_EQ(r->message, "disk at 91% on /var");
}

TEST(Part2Pipelines, IgnoresSurroundingWhitespace) {
    auto r = ir::parse_line("   2026-03-01T12:05:00Z ERROR web-2 boom   \t");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->server, "web-2");
    EXPECT_EQ(r->message, "boom");
}

TEST(Part2Pipelines, MessageKeepsInternalSpacing) {
    auto r = ir::parse_line("2026-03-01T12:05:00Z INFO db-1 two  spaces here");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->message, "two  spaces here");
}

TEST(Part2Pipelines, RejectsWrongFieldCount) {
    auto r = ir::parse_line("2026-03-01T12:05:00Z INFO web-1");
    ASSERT_FALSE(r.has_value());
}

TEST(Part2Pipelines, RejectsNaiveTimestamp) {
    auto r = ir::parse_line("2026-03-01T12:05:00 INFO web-1 hi");
    ASSERT_FALSE(r.has_value());
}

TEST(Part2Pipelines, RejectsOffsetTimestamp) {
    auto r = ir::parse_line("2026-03-01T12:05:00+01:00 INFO web-1 hi");
    ASSERT_FALSE(r.has_value());
}

TEST(Part2Pipelines, RejectsUnknownOrLowercaseLevel) {
    EXPECT_FALSE(ir::parse_line("2026-03-01T12:05:00Z NOTICE web-1 hi").has_value());
    EXPECT_FALSE(ir::parse_line("2026-03-01T12:05:00Z info web-1 hi").has_value());
}

TEST(Part2Pipelines, ParseErrorsCarryNoLineNumberYet) {
    auto r = ir::parse_line("garbage");
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().lineno(), 0U);
    EXPECT_EQ(std::string(r.error().what()), r.error().reason());
}

TEST(Part2Pipelines, ParseLevelAcceptsExactNamesOnly) {
    EXPECT_EQ(ir::parse_level("DEBUG"), Level::debug);
    EXPECT_EQ(ir::parse_level("INFO"), Level::info);
    EXPECT_EQ(ir::parse_level("WARNING"), Level::warning);
    EXPECT_EQ(ir::parse_level("ERROR"), Level::error);
    EXPECT_EQ(ir::parse_level("CRITICAL"), Level::critical);
    EXPECT_EQ(ir::parse_level("Info"), std::nullopt);
    EXPECT_TRUE(Level::info < Level::error);
}

TEST(Part2Pipelines, AtLeastFiltersByThreshold) {
    ir::AtLeast pred(Level::warning);
    EXPECT_TRUE(pred(make(ts(0, 0, 0), Level::critical, "c")));
    EXPECT_TRUE(pred(make(ts(0, 0, 0), Level::warning, "w")));
    EXPECT_FALSE(pred(make(ts(0, 0, 0), Level::info, "i")));
}

TEST(Part2Pipelines, CollectAtLeastKeepsRecordsAtOrAboveThreshold) {
    std::vector<std::string> lines{
        "2026-03-01T12:00:00Z INFO web-1 a",
        "2026-03-01T12:00:01Z WARNING web-1 b",
        "2026-03-01T12:00:02Z ERROR web-1 c",
    };
    auto out = ir::collect_at_least(lines, Level::warning);
    ASSERT_EQ(out.size(), 2U);
    EXPECT_EQ(out[0].message, "b");
    EXPECT_EQ(out[1].message, "c");
}

TEST(Part2Pipelines, CollectAtLeastCountsBlankLinesForLineNumbers) {
    std::vector<std::string> lines{
        "",
        "   ",
        "2026-03-01T12:00:00Z ERROR web-1 ok",
        "not a record",
    };
    try {
        (void)ir::collect_at_least(lines, Level::debug);
        FAIL() << "expected LogParseError";
    } catch (const ir::LogParseError& e) {
        EXPECT_EQ(e.lineno(), 4U);
        EXPECT_EQ(std::string(e.what()).rfind("line 4: ", 0), 0U);
    }
}

TEST(Part2Pipelines, CollectAtLeastStopsAtFirstMalformedLine) {
    std::vector<std::string> lines{
        "2026-03-01T12:00:00Z INFO web-1 good",
        "bad line",
        "2026-03-01T12:00:02Z INFO web-1 never reached",
        "2026-03-01T12:00:03Z INFO web-1 never reached",
    };
    int reads = 0;
    auto counted = lines | views::transform([&reads](const std::string& s) {
                       ++reads;
                       return std::string_view{s};
                   });
    EXPECT_THROW((void)ir::collect_at_least(counted, Level::debug), ir::LogParseError);
    EXPECT_EQ(reads, 2);
}

TEST(Part2Pipelines, FirstErrorFindsErrorOrCritical) {
    std::vector<LogRecord> records{
        make(ts(0, 0, 0), Level::info, "i"),
        make(ts(0, 0, 1), Level::warning, "w"),
        make(ts(0, 0, 2), Level::error, "first"),
        make(ts(0, 0, 3), Level::critical, "second"),
    };
    auto it = ir::first_error(records);
    ASSERT_NE(it, records.end());
    EXPECT_EQ(it->message, "first");
}

TEST(Part2Pipelines, FirstErrorReturnsEndWhenNoErrors) {
    std::vector<LogRecord> records{
        make(ts(0, 0, 0), Level::info, "i"),
        make(ts(0, 0, 1), Level::warning, "w"),
    };
    EXPECT_EQ(ir::first_error(records), records.end());
    std::vector<LogRecord> empty;
    EXPECT_EQ(ir::first_error(empty), empty.end());
}

TEST(Part2Pipelines, FirstErrorStopsConsumingAtMatch) {
    std::vector<LogRecord> records{
        make(ts(0, 0, 0), Level::info, "i"),
        make(ts(0, 0, 1), Level::error, "e"),
        make(ts(0, 0, 2), Level::info, "i2"),
        make(ts(0, 0, 3), Level::info, "i3"),
    };
    int reads = 0;
    auto counted = records | views::transform([&reads](const LogRecord& r) {
                       ++reads;
                       return r;
                   });
    auto it = ir::first_error(counted);
    EXPECT_EQ(reads, 2);
    EXPECT_EQ((*it).message, "e");
}

TEST(Part2Pipelines, FirstErrorReturnsDanglingForTemporaries) {
    // A temporary range cannot hand out an iterator that outlives the call.
    static_assert(std::same_as<decltype(ir::first_error(std::vector<LogRecord>{})),
                               std::ranges::dangling>);
    std::vector<LogRecord> records{make(ts(0, 0, 0), Level::error, "e")};
    static_assert(std::same_as<decltype(ir::first_error(records)),
                               std::vector<LogRecord>::iterator>);
    EXPECT_EQ(ir::first_error(records)->message, "e");
}
