// Part 1: zero-allocation log-line parsing and level counting.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>

namespace cppref::performance {

enum class LogLevel : std::uint8_t { trace, debug, info, warn, error };

// Line grammar (one line, no trailing newline):
//   <timestamp> <LEVEL> <component> status=<u32> latency_us=<u64>[ <message>]
// Tokens are separated by exactly one space. LEVEL is TRACE, DEBUG, INFO, WARN or ERROR
// (case-sensitive). The message is the rest of the line verbatim: it may contain spaces and may
// be empty (the line then ends after latency_us, or after a single trailing space).
// Every view in LogLine aliases the input line.
struct LogLine {
    std::string_view timestamp;  // first token; only checked to be non-empty
    LogLevel level;
    std::string_view component;  // dotted, e.g. "api.http"
    std::uint32_t status;
    std::uint64_t latency_us;
    std::string_view message;
};

enum class ParseErrorKind : std::uint8_t {
    missing_field,  // token absent, or wrong key (status= / latency_us=)
    bad_level,      // level token is not one of the five names
    bad_number,     // empty, signed, non-digit, or out of range for its field
};

struct ParseError {
    ParseErrorKind kind;
    // Byte offset of the offending token (missing_field, bad_level), or of the digits (bad_number).
    // For a field missing at the end of the line this is line.size().
    std::size_t column;
};

// Parses one line. Never allocates.
std::expected<LogLine, ParseError> parse_line(std::string_view line);

// "TRACE", "DEBUG", "INFO", "WARN", "ERROR". Never allocates.
std::string_view to_string(LogLevel level);

struct LevelCounts {
    std::array<std::size_t, 5> by_level{};  // indexed by static_cast<std::size_t>(LogLevel)
    std::size_t malformed = 0;
};

// Splits `log` on '\n', strips one trailing '\r' per line, skips empty lines, parses every other
// line and tallies it. Never allocates.
LevelCounts count_levels(std::string_view log);

}  // namespace cppref::performance
