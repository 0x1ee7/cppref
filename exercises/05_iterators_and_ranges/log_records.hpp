// Engineer-owned. Part 2: log records, line parsing, and predicate/range helpers.
#pragma once

#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace cppref::iterators_and_ranges {

/// Severity, least to most severe: `a < b` means `a` is less severe than `b`.
enum class Level : std::uint8_t { debug, info, warning, error, critical };

/// Parses the exact names "DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL".
/// Anything else yields `std::nullopt`.
std::optional<Level> parse_level(std::string_view text);

/// One parsed log line. `ts` is UTC with whole-second resolution.
struct LogRecord {
    std::chrono::sys_seconds ts{};
    Level level{Level::info};
    std::string server;
    std::string message;

    friend bool operator==(const LogRecord&, const LogRecord&) = default;
};

/// A malformed line. `lineno` is the 1-based line number when known, 0 otherwise.
/// `what()` is "line N: reason" when `lineno > 0`, otherwise just "reason".
class LogParseError : public std::runtime_error {
public:
    LogParseError(std::string reason, std::size_t lineno);

    [[nodiscard]] std::size_t lineno() const;
    [[nodiscard]] const std::string& reason() const;

private:
    // engineer: store the reason and the line number
};

/// Parses `2026-03-01T12:05:00Z WARNING web-1 disk at 91% on /var`.
///
/// Rules:
/// - surrounding whitespace is ignored; the message keeps its internal spacing;
/// - exactly four fields: timestamp, level, server, message (message must be non-empty);
/// - the timestamp must be UTC `YYYY-MM-DDTHH:MM:SSZ`; naive and offset forms are rejected;
/// - the level must be one of the names accepted by parse_level.
///
/// Errors carry `lineno() == 0`; collect_at_least() assigns line numbers.
std::expected<LogRecord, LogParseError> parse_line(std::string_view line);

/// Predicate for `views::filter`: true when `record.level >= min`. Holds the threshold by value.
class AtLeast {
public:
    explicit AtLeast(Level min);

    [[nodiscard]] bool operator()(const LogRecord& record) const;

private:
    Level min_{Level::debug};  // engineer: the threshold; adjust if you prefer another layout
};

/// Parses every line of `lines` in one pass and returns the records at or above `min`.
/// Blank or whitespace-only lines are skipped but still counted for line numbers.
/// On the first malformed line, throws LogParseError whose `lineno()` is that line's 1-based
/// position in `lines`; nothing past that line is read.
template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_reference_t<R>, std::string_view>
std::vector<LogRecord> collect_at_least(R&& lines, Level min) {
    throw std::logic_error("not implemented: collect_at_least");
}

/// Iterator to the first record whose level is `error` or `critical`, or the end iterator.
/// Stops consuming as soon as it matches.
///
/// For a non-borrowed rvalue (temporary) range the return type is `std::ranges::dangling`,
/// so the caller cannot keep an iterator into a temporary.
template <std::ranges::input_range R>
    requires std::same_as<std::ranges::range_value_t<R>, LogRecord>
std::ranges::borrowed_iterator_t<R> first_error(R&& records) {
    throw std::logic_error("not implemented: first_error");
}

}  // namespace cppref::iterators_and_ranges
