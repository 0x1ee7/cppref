// Part 1 tests: zero-allocation log-line parsing and level counting.
#include "log_parser.hpp"
#include "perf_support_test.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace {

using cppref::performance::count_levels;
using cppref::performance::LevelCounts;
using cppref::performance::LogLevel;
using cppref::performance::ParseError;
using cppref::performance::ParseErrorKind;
using cppref::performance::parse_line;
using cppref::performance::to_string;
using cppref::performance::testing::AllocScope;

constexpr std::string_view kLine = "2024-05-01T10:00:00Z INFO api.http status=200 latency_us=15 ok";

// A valid line with the given level token.
std::string line_with_level(std::string_view level) {
    std::string out = "2024-05-01T10:00:00Z ";
    out += level;
    out += " api.http status=200 latency_us=15 ok";
    return out;
}

// Parses a line that must be rejected and returns the error.
ParseError error_of(std::string_view text) {
    auto result = parse_line(text);
    if (result.has_value()) {
        ADD_FAILURE() << "expected a parse error for: " << text;
        return ParseError{ParseErrorKind::missing_field, 0};
    }
    return result.error();
}

TEST(Part1Parse, ParsesEveryField) {
    auto r = parse_line(kLine);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->timestamp, "2024-05-01T10:00:00Z");
    EXPECT_EQ(r->level, LogLevel::info);
    EXPECT_EQ(r->component, "api.http");
    EXPECT_EQ(r->status, 200u);
    EXPECT_EQ(r->latency_us, 15u);
    EXPECT_EQ(r->message, "ok");
}

TEST(Part1Parse, MessageKeepsInternalSpaces) {
    auto r = parse_line("2024-05-01T10:00:00Z WARN api.http status=503 latency_us=2 disk almost full");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->level, LogLevel::warn);
    EXPECT_EQ(r->status, 503u);
    EXPECT_EQ(r->message, "disk almost full");
}

TEST(Part1Parse, LineEndingAfterLatencyHasEmptyMessage) {
    auto r = parse_line("2024-05-01T10:00:00Z INFO api.http status=200 latency_us=15");
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->message.empty());
    EXPECT_EQ(r->latency_us, 15u);
}

TEST(Part1Parse, TrailingSpaceGivesEmptyMessage) {
    auto r = parse_line("2024-05-01T10:00:00Z INFO api.http status=200 latency_us=15 ");
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->message.empty());
}

TEST(Part1Parse, AcceptsEveryLevelName) {
    const std::pair<std::string_view, LogLevel> cases[] = {
        {"TRACE", LogLevel::trace}, {"DEBUG", LogLevel::debug}, {"INFO", LogLevel::info},
        {"WARN", LogLevel::warn},   {"ERROR", LogLevel::error},
    };
    for (const auto& [name, level] : cases) {
        auto r = parse_line(line_with_level(name));
        ASSERT_TRUE(r.has_value()) << name;
        EXPECT_EQ(r->level, level) << name;
    }
}

TEST(Part1Parse, AcceptsNumericBoundaries) {
    auto r = parse_line(
        "2024-05-01T10:00:00Z ERROR api.http status=4294967295 latency_us=18446744073709551615 ok");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->status, 4294967295u);
    EXPECT_EQ(r->latency_us, 18446744073709551615ull);
}

TEST(Part1Parse, ViewsAliasTheInputLine) {
    auto r = parse_line(kLine);
    ASSERT_TRUE(r.has_value());
    const char* begin = kLine.data();
    const char* end = begin + kLine.size();
    EXPECT_GE(r->component.data(), begin);
    EXPECT_LE(r->component.data() + r->component.size(), end);
    EXPECT_GE(r->message.data(), begin);
    EXPECT_LE(r->message.data() + r->message.size(), end);
}

TEST(Part1Errors, UnknownLevelReportsItsOffset) {
    const ParseError e = error_of(line_with_level("NOTICE"));
    EXPECT_EQ(e.kind, ParseErrorKind::bad_level);
    EXPECT_EQ(e.column, 21u);
}

TEST(Part1Errors, LevelIsCaseSensitive) {
    const ParseError e = error_of(line_with_level("info"));
    EXPECT_EQ(e.kind, ParseErrorKind::bad_level);
    EXPECT_EQ(e.column, 21u);
}

TEST(Part1Errors, MissingStatusKeyIsReported) {
    const ParseError e =
        error_of("2024-05-01T10:00:00Z INFO api.http latency_us=15 ok");
    EXPECT_EQ(e.kind, ParseErrorKind::missing_field);
    EXPECT_EQ(e.column, 35u);
}

TEST(Part1Errors, MissingLatencyAtEndOfLineReportsLineSize) {
    const ParseError e = error_of("2024-05-01T10:00:00Z INFO api.http status=200");
    EXPECT_EQ(e.kind, ParseErrorKind::missing_field);
    EXPECT_EQ(e.column, 45u);
}

TEST(Part1Errors, NegativeStatusIsBadNumber) {
    const ParseError e =
        error_of("2024-05-01T10:00:00Z INFO api.http status=-1 latency_us=15 ok");
    EXPECT_EQ(e.kind, ParseErrorKind::bad_number);
}

TEST(Part1Errors, StatusOutOfRangeIsBadNumber) {
    const ParseError e = error_of(
        "2024-05-01T10:00:00Z INFO api.http status=4294967296 latency_us=15 ok");
    EXPECT_EQ(e.kind, ParseErrorKind::bad_number);
    EXPECT_EQ(e.column, 42u);
}

TEST(Part1Errors, NonDigitStatusIsBadNumber) {
    const ParseError e = error_of("2024-05-01T10:00:00Z INFO api.http status=20x latency_us=15 ok");
    EXPECT_EQ(e.kind, ParseErrorKind::bad_number);
    EXPECT_EQ(e.column, 42u);
}

TEST(Part1Errors, EmptyLatencyIsBadNumber) {
    const ParseError e = error_of("2024-05-01T10:00:00Z INFO api.http status=200 latency_us= ok");
    EXPECT_EQ(e.kind, ParseErrorKind::bad_number);
    EXPECT_EQ(e.column, 57u);
}

TEST(Part1Errors, LatencyOutOfRangeIsBadNumber) {
    const ParseError e = error_of(
        "2024-05-01T10:00:00Z INFO api.http status=200 latency_us=18446744073709551616 ok");
    EXPECT_EQ(e.kind, ParseErrorKind::bad_number);
    EXPECT_EQ(e.column, 57u);
}

TEST(Part1Levels, NamesMatchLevels) {
    EXPECT_EQ(to_string(LogLevel::trace), "TRACE");
    EXPECT_EQ(to_string(LogLevel::debug), "DEBUG");
    EXPECT_EQ(to_string(LogLevel::info), "INFO");
    EXPECT_EQ(to_string(LogLevel::warn), "WARN");
    EXPECT_EQ(to_string(LogLevel::error), "ERROR");
}

TEST(Part1Levels, CountsLevelsSkipsBlanksAndCountsMalformed) {
    std::string log;
    log += line_with_level("INFO") + "\n";
    log += line_with_level("INFO") + "\n";
    log += line_with_level("ERROR") + "\n";
    log += "\n";
    log += "not a log line\n";
    log += line_with_level("WARN") + "\n";

    const LevelCounts counts = count_levels(log);
    EXPECT_EQ(counts.by_level[static_cast<std::size_t>(LogLevel::info)], 2u);
    EXPECT_EQ(counts.by_level[static_cast<std::size_t>(LogLevel::error)], 1u);
    EXPECT_EQ(counts.by_level[static_cast<std::size_t>(LogLevel::warn)], 1u);
    EXPECT_EQ(counts.by_level[static_cast<std::size_t>(LogLevel::trace)], 0u);
    EXPECT_EQ(counts.malformed, 1u);
}

TEST(Part1Levels, StripsOneTrailingCarriageReturn) {
    const std::string log = line_with_level("WARN") + "\r\n" + line_with_level("WARN") + "\n";
    const LevelCounts counts = count_levels(log);
    EXPECT_EQ(counts.by_level[static_cast<std::size_t>(LogLevel::warn)], 2u);
    EXPECT_EQ(counts.malformed, 0u);
}

TEST(Part1Alloc, ParseLineNeverAllocates) {
    ASSERT_TRUE(parse_line(kLine).has_value());
    AllocScope scope;
    bool ok = true;
    for (int i = 0; i < 100; ++i) ok = ok && parse_line(kLine).has_value();
    const std::size_t allocs = scope.allocations();
    EXPECT_TRUE(ok);
    EXPECT_EQ(allocs, 0u);
}

TEST(Part1Alloc, CountLevelsNeverAllocates) {
    std::string log;
    for (int i = 0; i < 200; ++i) log += line_with_level("INFO") + "\n";
    AllocScope scope;
    const LevelCounts counts = count_levels(log);
    const std::size_t allocs = scope.allocations();
    EXPECT_EQ(counts.by_level[static_cast<std::size_t>(LogLevel::info)], 200u);
    EXPECT_EQ(allocs, 0u);
}

}  // namespace
