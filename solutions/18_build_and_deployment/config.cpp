#include "config.hpp"

#include "build_info.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <system_error>
#include <utility>

namespace cppref::deployment {
namespace {

bool is_space(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }

std::string_view trim(std::string_view s) {
    while (!s.empty() && is_space(s.front())) s.remove_prefix(1);
    while (!s.empty() && is_space(s.back())) s.remove_suffix(1);
    return s;
}

std::string lower(std::string_view s) {
    std::string out(s);
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

std::string join_errors(const std::vector<std::string>& errors) {
    std::string out = "invalid configuration: ";
    for (std::size_t i = 0; i < errors.size(); ++i) {
        if (i > 0) out += "; ";
        out += errors[i];
    }
    return out;
}

// Empty or whitespace-only values are treated as unset.
std::optional<std::string> value_of(const EnvLookup& env, std::string_view name) {
    auto raw = env(name);
    if (!raw) return std::nullopt;
    auto t = trim(*raw);
    if (t.empty()) return std::nullopt;
    return std::string(t);
}

std::string line_error(std::size_t line, std::string_view message) {
    return "line " + std::to_string(line) + ": " + std::string(message);
}

bool valid_key(std::string_view key) {
    if (key.empty()) return false;
    const auto first = static_cast<unsigned char>(key.front());
    if (!(std::isalpha(first) || key.front() == '_')) return false;
    return std::ranges::all_of(key, [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
    });
}

}  // namespace

ConfigError::ConfigError(std::vector<std::string> errors)
    : std::runtime_error(join_errors(errors)), errors_(std::move(errors)) {}

const std::vector<std::string>& ConfigError::errors() const { return errors_; }

std::expected<LogLevel, std::string> parse_log_level(std::string_view text) {
    const auto t = lower(trim(text));
    if (t == "debug") return LogLevel::debug;
    if (t == "info") return LogLevel::info;
    if (t == "warning") return LogLevel::warning;
    if (t == "error") return LogLevel::error;
    if (t == "critical") return LogLevel::critical;
    return std::unexpected("unknown log level '" + std::string(trim(text)) + "'");
}

std::string_view to_string(LogLevel level) {
    switch (level) {
        case LogLevel::debug: return "DEBUG";
        case LogLevel::info: return "INFO";
        case LogLevel::warning: return "WARNING";
        case LogLevel::error: return "ERROR";
        case LogLevel::critical: return "CRITICAL";
    }
    return "UNKNOWN";
}

Config Config::from_env(const EnvLookup& env) {
    std::vector<std::string> errors;
    auto fail = [&errors](std::string_view var, std::string_view why) {
        errors.push_back(std::string(var) + ": " + std::string(why));
    };
    Config cfg;

    if (auto url = value_of(env, "DATABASE_URL")) {
        if (url->starts_with("postgresql://") || url->starts_with("sqlite:")) {
            cfg.database_url = *url;
        } else {
            fail("DATABASE_URL", "scheme must be postgresql:// or sqlite:");
        }
    } else {
        fail("DATABASE_URL", "required but not set");
    }

    if (auto token = value_of(env, "API_TOKEN")) {
        if (token->size() >= 16) {
            cfg.api_token = *token;
        } else {
            fail("API_TOKEN", "must be at least 16 characters");
        }
    } else {
        fail("API_TOKEN", "required but not set");
    }

    if (auto name = value_of(env, "APP_NAME")) cfg.app_name = *name;

    if (auto port = value_of(env, "PORT")) {
        unsigned long value = 0;
        const auto [ptr, ec] = std::from_chars(port->data(), port->data() + port->size(), value);
        if (ec == std::errc{} && ptr == port->data() + port->size() && value >= 1 && value <= 65535) {
            cfg.port = static_cast<std::uint16_t>(value);
        } else {
            fail("PORT", "must be an integer 1-65535");
        }
    }

    if (auto debug = value_of(env, "DEBUG")) {
        if (auto parsed = parse_bool(*debug)) {
            cfg.debug = *parsed;
        } else {
            fail("DEBUG", "must be a boolean (true/false/yes/no/on/off/1/0)");
        }
    }

    if (auto hosts = value_of(env, "ALLOWED_HOSTS")) cfg.allowed_hosts = parse_list(*hosts);

    if (auto timeout = value_of(env, "REQUEST_TIMEOUT")) {
        if (auto parsed = parse_duration(*timeout); parsed && parsed->count() > 0) {
            cfg.request_timeout = *parsed;
        } else {
            fail("REQUEST_TIMEOUT", "must be a duration greater than zero");
        }
    }

    if (auto level = value_of(env, "LOG_LEVEL")) {
        if (auto parsed = parse_log_level(*level)) {
            cfg.log_level = *parsed;
        } else {
            fail("LOG_LEVEL", "must be one of DEBUG, INFO, WARNING, ERROR, CRITICAL");
        }
    }

    if (auto sha = value_of(env, "GIT_SHA")) {
        auto lowered = lower(*sha);
        if (is_git_sha(lowered)) {
            cfg.git_sha = std::move(lowered);
        } else {
            fail("GIT_SHA", "must be 7-40 lowercase hex characters");
        }
    }

    if (!errors.empty()) throw ConfigError(std::move(errors));
    return cfg;
}

std::string redacted(const Config& config) {
    std::string hosts;
    for (std::size_t i = 0; i < config.allowed_hosts.size(); ++i) {
        if (i > 0) hosts += ",";
        hosts += config.allowed_hosts[i];
    }
    return "Config{app_name=" + config.app_name +
           ", database_url=" + mask_url_password(config.database_url) +
           ", api_token=***, port=" + std::to_string(config.port) +
           ", debug=" + (config.debug ? "true" : "false") +
           ", allowed_hosts=[" + hosts + "]" +
           ", request_timeout_ms=" + std::to_string(config.request_timeout.count()) +
           ", log_level=" + std::string(to_string(config.log_level)) +
           ", git_sha=" + (config.git_sha ? *config.git_sha : std::string("none")) + "}";
}

std::expected<bool, std::string> parse_bool(std::string_view text) {
    const auto t = lower(trim(text));
    if (t == "1" || t == "true" || t == "yes" || t == "on") return true;
    if (t == "0" || t == "false" || t == "no" || t == "off") return false;
    return std::unexpected("not a boolean: '" + std::string(trim(text)) + "'");
}

std::expected<std::chrono::milliseconds, std::string> parse_duration(std::string_view text) {
    const auto t = trim(text);
    if (t.empty()) return std::unexpected("empty duration");
    if (t.front() == '-') return std::unexpected("negative durations are not allowed");

    std::size_t i = 0;
    bool digits = false;
    bool dot = false;
    while (i < t.size() && (std::isdigit(static_cast<unsigned char>(t[i])) || (t[i] == '.' && !dot))) {
        if (t[i] == '.') {
            dot = true;
        } else {
            digits = true;
        }
        ++i;
    }
    if (!digits) return std::unexpected("expected a number in '" + std::string(t) + "'");

    const auto number = t.substr(0, i);
    double value = 0.0;
    const auto [ptr, ec] = std::from_chars(number.data(), number.data() + number.size(), value);
    if (ec != std::errc{} || ptr != number.data() + number.size()) {
        return std::unexpected("invalid number '" + std::string(number) + "'");
    }

    const auto unit = t.substr(i);
    double factor = 0.0;
    if (unit.empty() || unit == "s") {
        factor = 1000.0;
    } else if (unit == "ms") {
        factor = 1.0;
    } else if (unit == "m") {
        factor = 60000.0;
    } else if (unit == "h") {
        factor = 3600000.0;
    } else {
        return std::unexpected("unknown unit '" + std::string(unit) + "'");
    }

    const double ms = value * factor;
    if (!(ms < 9.0e15)) return std::unexpected("duration too large");
    return std::chrono::milliseconds(static_cast<std::chrono::milliseconds::rep>(std::llround(ms)));
}

std::vector<std::string> parse_list(std::string_view text) {
    std::vector<std::string> out;
    while (!text.empty()) {
        const auto comma = text.find(',');
        const auto item = trim(text.substr(0, comma));
        if (!item.empty()) out.emplace_back(item);
        if (comma == std::string_view::npos) break;
        text.remove_prefix(comma + 1);
    }
    return out;
}

std::string mask_url_password(std::string_view url) {
    const auto scheme = url.find("://");
    if (scheme == std::string_view::npos) return std::string(url);

    const auto auth_begin = scheme + 3;
    auto auth_end = url.find_first_of("/?#", auth_begin);
    if (auth_end == std::string_view::npos) auth_end = url.size();

    const auto authority = url.substr(auth_begin, auth_end - auth_begin);
    const auto at = authority.rfind('@');
    if (at == std::string_view::npos) return std::string(url);

    const auto colon = authority.substr(0, at).find(':');
    if (colon == std::string_view::npos) return std::string(url);

    const auto colon_pos = auth_begin + colon;
    const auto at_pos = auth_begin + at;
    return std::string(url.substr(0, colon_pos + 1)) + "***" + std::string(url.substr(at_pos));
}

std::expected<DotenvMap, std::string> load_dotenv(std::string_view text) {
    DotenvMap out;
    std::size_t line_no = 0;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const auto nl = text.find('\n', pos);
        const auto raw = text.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos);
        pos = nl == std::string_view::npos ? text.size() : nl + 1;
        ++line_no;

        auto line = trim(raw);
        if (line.empty() || line.front() == '#') continue;
        if (line.starts_with("export") && line.size() > 6 && is_space(line[6])) {
            line = trim(line.substr(6));
        }

        const auto eq = line.find('=');
        if (eq == std::string_view::npos) return std::unexpected(line_error(line_no, "expected KEY=VALUE"));
        const auto key = trim(line.substr(0, eq));
        if (!valid_key(key)) {
            return std::unexpected(line_error(line_no, "invalid key '" + std::string(key) + "'"));
        }

        const auto rest = trim(line.substr(eq + 1));
        std::string value;
        if (!rest.empty() && (rest.front() == '"' || rest.front() == '\'')) {
            const char quote = rest.front();
            std::size_t i = 1;
            bool closed = false;
            for (; i < rest.size(); ++i) {
                const char c = rest[i];
                if (quote == '"' && c == '\\' && i + 1 < rest.size()) {
                    const char e = rest[++i];
                    switch (e) {
                        case 'n': value += '\n'; break;
                        case 't': value += '\t'; break;
                        case '"': value += '"'; break;
                        case '\\': value += '\\'; break;
                        default: value += '\\'; value += e; break;
                    }
                    continue;
                }
                if (c == quote) {
                    closed = true;
                    ++i;
                    break;
                }
                value += c;
            }
            if (!closed) return std::unexpected(line_error(line_no, "unterminated quote"));
            const auto tail = trim(rest.substr(i));
            if (!tail.empty() && tail.front() != '#') {
                return std::unexpected(line_error(line_no, "unexpected text after closing quote"));
            }
        } else {
            std::size_t end = rest.size();
            for (std::size_t i = 0; i < rest.size(); ++i) {
                if (rest[i] == '#' && (i == 0 || is_space(rest[i - 1]))) {
                    end = i;
                    break;
                }
            }
            value = std::string(trim(rest.substr(0, end)));
        }
        out[std::string(key)] = std::move(value);
    }
    return out;
}

EnvLookup with_dotenv(EnvLookup env, DotenvMap dotenv) {
    return [env = std::move(env), dotenv = std::move(dotenv)](std::string_view name) -> std::optional<std::string> {
        if (auto live = env(name)) return live;
        auto it = dotenv.find(name);
        if (it == dotenv.end()) return std::nullopt;
        return it->second;
    };
}

}  // namespace cppref::deployment
