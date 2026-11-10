// Part 2: log records, line parsing, and predicate/range helpers.
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
#include <utility>
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
class LogParseError : public std::runtime_error {
public:
    LogParseError(std::string reason, std::size_t lineno);

    [[nodiscard]] std::size_t lineno() const { return lineno_; }
    [[nodiscard]] const std::string& reason() const { return reason_; }

private:
    std::string reason_;
    std::size_t lineno_;
};

/// Parses `2026-03-01T12:05:00Z WARNING web-1 disk at 91% on /var`.
/// Errors carry `lineno() == 0`; collect_at_least() assigns line numbers.
std::expected<LogRecord, LogParseError> parse_line(std::string_view line);

/// Predicate for `views::filter`: true when `record.level >= min`.
class AtLeast {
public:
    explicit AtLeast(Level min) : min_(min) {}

    [[nodiscard]] bool operator()(const LogRecord& record) const { return record.level >= min_; }

private:
    Level min_{Level::debug};
};

/// Parses every line of `lines` in one pass and returns the records at or above `min`.
/// Blank lines are skipped but still counted for line numbers.
template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_reference_t<R>, std::string_view>
std::vector<LogRecord> collect_at_least(R&& lines, Level min) {
    const AtLeast keep(min);
    std::vector<LogRecord> out;
    std::size_t lineno = 0;
    for (auto&& line : lines) {
        ++lineno;
        const std::string_view text = line;
        if (text.find_first_not_of(" \t\r\n\f\v") == std::string_view::npos) {
            continue;
        }
        auto parsed = parse_line(text);
        if (!parsed) {
            throw LogParseError(parsed.error().reason(), lineno);
        }
        if (keep(*parsed)) {
            out.push_back(std::move(*parsed));
        }
    }
    return out;
}

/// Iterator to the first record whose level is `error` or `critical`, or the end iterator.
/// Stops consuming as soon as it matches. Temporaries yield `std::ranges::dangling`.
template <std::ranges::input_range R>
    requires std::same_as<std::ranges::range_value_t<R>, LogRecord>
std::ranges::borrowed_iterator_t<R> first_error(R&& records) {
    auto it = std::ranges::begin(records);
    const auto last = std::ranges::end(records);
    while (it != last && (*it).level < Level::error) {
        ++it;
    }
    if constexpr (std::ranges::borrowed_range<R>) {
        return it;
    } else {
        return std::ranges::dangling{};
    }
}

}  // namespace cppref::iterators_and_ranges
