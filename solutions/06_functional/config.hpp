#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

// Part 5 · Exception-free config parsing (engineer-owned).
//
// Parsers never throw. Failures are values; parse_config accumulates every
// error it finds instead of stopping at the first one.
namespace cppref::functional {

struct Config {
    std::string host;
    std::uint16_t port = 0;
    bool debug = false;
    int workers = 4;

    friend bool operator==(const Config&, const Config&) = default;
};

/// Trims surrounding whitespace, then parses an integer in 1..65535.
/// Non-integer -> "port must be an integer, got '<trimmed>'".
/// Out of range -> "port must be between 1 and 65535".
[[nodiscard]] std::expected<std::uint16_t, std::string> parse_port(std::string_view raw);

/// Comma-separated ports. Returns the first port error (via collect).
[[nodiscard]] std::expected<std::vector<std::uint16_t>, std::string> parse_ports(
    std::string_view raw);

/// Case-insensitive: true/yes/1/on and false/no/0/off. Anything else ->
/// "expected boolean, got '<trimmed>'".
[[nodiscard]] std::expected<bool, std::string> parse_bool(std::string_view raw);

/// Parses `key = value` lines. Blank lines and lines starting with '#' are
/// skipped. Every error is collected, in order, as a message:
///  "line N: expected 'key = value'"           line without '='
///  "line N: unknown key 'k'"                  key not in host/port/debug/workers
///  "line N: duplicate key 'k'"                second occurrence of a key
///  "line N: host must not be empty"
///  "line N: <parse_port message>" / "line N: <parse_bool message>"
///  "line N: workers must be between 1 and 64" (also for non-integers)
///  "missing required key 'host'" / "missing required key 'port'" (after the scan)
/// Returns Config only when the error list is empty.
[[nodiscard]] std::expected<Config, std::vector<std::string>> parse_config(std::string_view text);

}  // namespace cppref::functional
