#pragma once

#include "errors.hpp"

#include <charconv>
#include <concepts>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace cppref::error_handling {

/// Flat settings: dotted key ("db.pool.size") -> raw string value.
using Config = std::map<std::string, std::string, std::less<>>;

/// Syntax error raised by the config parser. what() is "line N: <detail>".
/// load_config never lets it escape: it is nested under ConfigInvalid.
class ParseError : public std::runtime_error {
public:
    ParseError(std::size_t line, const std::string& detail);

    /// 1-based number of the offending line.
    [[nodiscard]] std::size_t line() const;

private:
    std::size_t line_;
};

/// Parses INI-style text: `[section]` headers (dotted names nest), `key = value`
/// lines, `#` comments and blank lines. Keys are flattened to "section.key".
/// Keys before the first section are root keys. Duplicate keys are an error.
/// Throws ParseError on the first bad line.
[[nodiscard]] Config parse_config(std::string_view text);

/// Reads `path` and parses it. Translates failures into ConfigError subclasses,
/// nesting the original error with std::throw_with_nested:
///   - missing file            -> ConfigNotFound, cause std::system_error
///   - directory / I/O failure -> ConfigUnreadable, cause std::system_error
///   - syntax error            -> ConfigInvalid, cause ParseError
[[nodiscard]] Config load_config(const std::filesystem::path& path);

/// Throws ConfigInvalid whose message names `key`, nested over the
/// std::invalid_argument that a failed conversion of `raw` produced.
[[noreturn]] void throw_invalid_setting(std::string_view key, std::string_view raw);

/// Looks up a dotted key and converts it. T is std::string or a non-bool integer.
/// Missing key       -> SettingMissing(key), plain throw (no nested cause).
/// Bad conversion    -> throw_invalid_setting(key, raw).
template <typename T>
    requires(std::same_as<T, std::string> || (std::integral<T> && !std::same_as<T, bool>))
[[nodiscard]] T get_setting([[maybe_unused]] const Config& config, [[maybe_unused]] std::string_view key) {
    throw std::logic_error("not implemented: get_setting");
}

// --- Boundary translation: exceptions <-> std::expected ----------------------

/// Why try_load_config failed.
enum class ConfigFailure { not_found, unreadable, invalid };

/// Exceptions -> expected at an API boundary. Maps the three ConfigError
/// subclasses from load_config to ConfigFailure. Other exceptions propagate.
[[nodiscard]] std::expected<Config, ConfigFailure> try_load_config(const std::filesystem::path& path);

/// expected -> exceptions. On failure throws ConfigNotFound / ConfigUnreadable /
/// ConfigInvalid whose message contains `path`; there is no nested cause because
/// nothing was caught. Returns the config on success.
[[nodiscard]] Config value_or_throw(std::expected<Config, ConfigFailure> result, const std::filesystem::path& path);

}  // namespace cppref::error_handling
