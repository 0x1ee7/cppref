#include "log_records.hpp"

#include <charconv>
#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace cppref::iterators_and_ranges {

namespace {

constexpr std::string_view kSpace = " \t\r\n\f\v";

std::string make_what(const std::string& reason, std::size_t lineno) {
    if (lineno > 0) {
        return "line " + std::to_string(lineno) + ": " + reason;
    }
    return reason;
}

std::string_view trim(std::string_view s) {
    const auto first = s.find_first_not_of(kSpace);
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = s.find_last_not_of(kSpace);
    return s.substr(first, last - first + 1);
}

// Pops the next whitespace-delimited token; `rest` keeps everything after it.
std::string_view next_token(std::string_view& rest) {
    const auto start = rest.find_first_not_of(kSpace);
    if (start == std::string_view::npos) {
        rest = {};
        return {};
    }
    rest.remove_prefix(start);
    const auto token = rest.substr(0, rest.find_first_of(kSpace));
    rest.remove_prefix(token.size());
    return token;
}

bool all_digits(std::string_view s) {
    if (s.empty()) {
        return false;
    }
    for (char c : s) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    return true;
}

int to_int(std::string_view digits) {
    int value = 0;
    std::from_chars(digits.data(), digits.data() + digits.size(), value);
    return value;
}

// Strict `YYYY-MM-DDTHH:MM:SSZ`; anything else is rejected.
std::optional<std::chrono::sys_seconds> parse_timestamp(std::string_view s) {
    if (s.size() != 20 || s[4] != '-' || s[7] != '-' || s[10] != 'T' || s[13] != ':' ||
        s[16] != ':' || s[19] != 'Z') {
        return std::nullopt;
    }
    const std::string_view parts[] = {s.substr(0, 4),  s.substr(5, 2),   s.substr(8, 2),
                                      s.substr(11, 2), s.substr(14, 2), s.substr(17, 2)};
    for (auto part : parts) {
        if (!all_digits(part)) {
            return std::nullopt;
        }
    }
    const int hour = to_int(s.substr(11, 2));
    const int minute = to_int(s.substr(14, 2));
    const int second = to_int(s.substr(17, 2));
    if (hour > 23 || minute > 59 || second > 59) {
        return std::nullopt;
    }
    using namespace std::chrono;
    const year_month_day ymd{year{to_int(s.substr(0, 4))},
                             month{static_cast<unsigned>(to_int(s.substr(5, 2)))},
                             day{static_cast<unsigned>(to_int(s.substr(8, 2)))}};
    if (!ymd.ok()) {
        return std::nullopt;
    }
    return sys_seconds{sys_days{ymd}} + hours{hour} + minutes{minute} + seconds{second};
}

}  // namespace

LogParseError::LogParseError(std::string reason, std::size_t lineno)
    : std::runtime_error(make_what(reason, lineno)), reason_(std::move(reason)), lineno_(lineno) {}

std::optional<Level> parse_level(std::string_view text) {
    if (text == "DEBUG") return Level::debug;
    if (text == "INFO") return Level::info;
    if (text == "WARNING") return Level::warning;
    if (text == "ERROR") return Level::error;
    if (text == "CRITICAL") return Level::critical;
    return std::nullopt;
}

std::expected<LogRecord, LogParseError> parse_line(std::string_view line) {
    auto fail = [](std::string reason) {
        return std::unexpected(LogParseError(std::move(reason), 0));
    };

    std::string_view rest = line;
    const auto ts_text = next_token(rest);
    const auto level_text = next_token(rest);
    const auto server_text = next_token(rest);
    const auto message = trim(rest);
    if (ts_text.empty() || level_text.empty() || server_text.empty() || message.empty()) {
        return fail("expected 4 fields: timestamp level server message");
    }

    const auto ts = parse_timestamp(ts_text);
    if (!ts) {
        return fail("invalid timestamp '" + std::string(ts_text) + "' (want UTC YYYY-MM-DDTHH:MM:SSZ)");
    }
    const auto level = parse_level(level_text);
    if (!level) {
        return fail("unknown level '" + std::string(level_text) + "'");
    }
    return LogRecord{*ts, *level, std::string(server_text), std::string(message)};
}

}  // namespace cppref::iterators_and_ranges
