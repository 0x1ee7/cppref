// Part 1 · 12-factor configuration: typed env parsing, .env grammar, secret masking.
#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace cppref::deployment {

/// Looks up one variable; nullopt = not set. Abstracts std::getenv so tests can inject values.
using EnvLookup = std::function<std::optional<std::string>(std::string_view)>;

/// Parsed .env file. The transparent comparator allows string_view lookups.
using DotenvMap = std::map<std::string, std::string, std::less<>>;

enum class LogLevel { debug, info, warning, error, critical };

/// Case-insensitive parse of "debug|info|warning|error|critical".
[[nodiscard]] std::expected<LogLevel, std::string> parse_log_level(std::string_view text);

/// Upper-case name: "DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL".
[[nodiscard]] std::string_view to_string(LogLevel level);

/// Thrown by Config::from_env. Carries every problem, one "VAR: problem" entry each.
/// what() == "invalid configuration: " + entries joined by "; ".
class ConfigError : public std::runtime_error {
public:
    explicit ConfigError(std::vector<std::string> errors);

    /// Every problem, in the order the variables were checked.
    [[nodiscard]] const std::vector<std::string>& errors() const;

private:
    std::vector<std::string> errors_;
};

/// Validated service settings. Built only through from_env().
struct Config {
    std::string database_url;                          // DATABASE_URL, required; postgresql:// or sqlite:
    std::string api_token;                             // API_TOKEN, required; secret, >= 16 bytes
    std::string app_name = "cppref-service";           // APP_NAME
    std::uint16_t port = 8080;                         // PORT, 1..65535
    bool debug = false;                                // DEBUG
    std::vector<std::string> allowed_hosts;            // ALLOWED_HOSTS, comma list
    std::chrono::milliseconds request_timeout{30'000}; // REQUEST_TIMEOUT, duration > 0
    LogLevel log_level = LogLevel::info;               // LOG_LEVEL
    std::optional<std::string> git_sha;                // GIT_SHA, 7..40 lowercase hex

    /// Reads and validates every variable. Collects ALL problems before throwing ConfigError.
    /// Empty or whitespace-only values count as unset.
    [[nodiscard]] static Config from_env(const EnvLookup& env);
};

/// Human-readable dump with secrets masked: token as "***", URL password masked.
[[nodiscard]] std::string redacted(const Config& config);

/// true: 1 true yes on; false: 0 false no off. Case-insensitive, whitespace stripped.
[[nodiscard]] std::expected<bool, std::string> parse_bool(std::string_view text);

/// "500ms", "30s", "5m", "1h", bare "2" (seconds), "1.5s". Rejects empty, negative, unknown unit, prose.
[[nodiscard]] std::expected<std::chrono::milliseconds, std::string> parse_duration(std::string_view text);

/// Comma-separated, each item stripped, empty items dropped.
[[nodiscard]] std::vector<std::string> parse_list(std::string_view text);

/// "postgresql://app:hunter2@db:5432/x" -> "postgresql://app:***@db:5432/x". No password: unchanged.
[[nodiscard]] std::string mask_url_password(std::string_view url);

/// Parses .env text. Errors read "line N: ...". A later duplicate key overrides an earlier one.
[[nodiscard]] std::expected<DotenvMap, std::string> load_dotenv(std::string_view text);

/// Layers a parsed .env file under the environment: the real environment always wins.
[[nodiscard]] EnvLookup with_dotenv(EnvLookup env, DotenvMap dotenv);

}  // namespace cppref::deployment
