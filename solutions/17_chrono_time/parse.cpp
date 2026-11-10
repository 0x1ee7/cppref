#include "parse.hpp"

#include <chrono>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>

namespace cppref::chrono_time {

namespace {

using namespace std::chrono;

constexpr std::int64_t kMsThreshold = 100'000'000'000;
constexpr std::int64_t kMinSeconds = -62'135'596'800;  // 0001-01-01T00:00:00Z
constexpr std::int64_t kMaxSeconds = 253'402'300'799;  // 9999-12-31T23:59:59Z
constexpr std::int64_t kMinMillis = kMinSeconds * 1000;
constexpr std::int64_t kMaxMillis = kMaxSeconds * 1000 + 999;

std::string_view trim(std::string_view s) {
    constexpr std::string_view ws = " \t\r\n\f\v";
    const auto first = s.find_first_not_of(ws);
    if (first == std::string_view::npos) {
        return {};
    }
    const auto end = s.find_last_not_of(ws);
    return s.substr(first, end - first + 1);
}

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

// [+-]? digits ( '.' digits )?  -- the whole string must match.
bool is_numeric(std::string_view s) {
    std::size_t i = 0;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
        ++i;
    }
    const std::size_t int_start = i;
    while (i < s.size() && is_digit(s[i])) {
        ++i;
    }
    if (i == int_start) {
        return false;
    }
    if (i == s.size()) {
        return true;
    }
    if (s[i] != '.') {
        return false;
    }
    ++i;
    const std::size_t frac_start = i;
    while (i < s.size() && is_digit(s[i])) {
        ++i;
    }
    return i == s.size() && i > frac_start;
}

// from_chars does not accept a leading '+', so strip it ourselves.
std::string_view strip_plus(std::string_view s) {
    if (!s.empty() && s.front() == '+') {
        s.remove_prefix(1);
    }
    return s;
}

void check_year_range(Instant t) {
    const auto d = std::chrono::floor<days>(t);
    const int y = static_cast<int>(year_month_day{d}.year());
    if (y < 1 || y > 9999) {
        throw std::out_of_range("timestamp outside years 0001..9999");
    }
}

Instant parse_rfc2822(std::string_view s) {
    if (s.ends_with("-0000")) {
        throw std::invalid_argument("RFC 2822 offset -0000 means unknown; rejected");
    }
    const std::string text(s);
    std::istringstream in(text);
    sys_time<milliseconds> tp;
    from_stream(in, "%a, %d %b %Y %H:%M:%S %z", tp);
    if (in.fail() || in.peek() != std::char_traits<char>::eof()) {
        throw std::invalid_argument("malformed RFC 2822 timestamp");
    }
    check_year_range(tp);
    return tp;
}

Instant parse_iso(std::string_view s) {
    std::string text(s);
    if (text.size() > 10 && text[10] == ' ') {
        text[10] = 'T';
    }
    // from_stream's %Ez does not accept a bare 'Z'; map it to the zero offset.
    if (text.ends_with('Z') || text.ends_with('z')) {
        text.replace(text.size() - 1, 1, "+00:00");
    }
    // sys_time normalises impossible dates (2026-02-30 -> March), so validate the date fields first.
    std::istringstream date_in(text.substr(0, 10));
    year_month_day date;
    from_stream(date_in, "%F", date);
    if (date_in.fail() || !date.ok()) {
        throw std::invalid_argument("invalid calendar date");
    }
    std::istringstream in(text);
    sys_time<milliseconds> tp;
    from_stream(in, "%FT%T%Ez", tp);
    if (in.fail() || in.peek() != std::char_traits<char>::eof()) {
        throw std::invalid_argument("malformed ISO 8601 timestamp (offset required)");
    }
    check_year_range(tp);
    return tp;
}

}  // namespace

Instant from_epoch_integer(std::int64_t value) {
    const bool millis = value >= kMsThreshold || value <= -kMsThreshold;
    if (millis) {
        if (value < kMinMillis || value > kMaxMillis) {
            throw std::out_of_range("epoch milliseconds outside years 0001..9999");
        }
        return Instant{milliseconds{value}};
    }
    if (value < kMinSeconds || value > kMaxSeconds) {
        throw std::out_of_range("epoch seconds outside years 0001..9999");
    }
    return Instant{seconds{value}};
}

Instant from_epoch_float(double value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("epoch value must be finite");
    }
    const bool millis = std::fabs(value) >= static_cast<double>(kMsThreshold);
    const double ms = std::round(millis ? value : value * 1000.0);  // ties away from zero
    if (ms < static_cast<double>(kMinMillis) || ms > static_cast<double>(kMaxMillis)) {
        throw std::out_of_range("epoch value outside years 0001..9999");
    }
    return Instant{milliseconds{static_cast<std::int64_t>(ms)}};
}

Instant parse_timestamp(std::string_view raw) {
    const std::string_view s = trim(raw);
    if (s.empty()) {
        throw std::invalid_argument("empty timestamp");
    }
    if (is_numeric(s)) {
        const std::string_view digits = strip_plus(s);
        const bool fractional = digits.find('.') != std::string_view::npos;
        if (fractional) {
            double v = 0.0;
            const auto res = std::from_chars(digits.data(), digits.data() + digits.size(), v);
            if (res.ec != std::errc{} || res.ptr != digits.data() + digits.size()) {
                throw std::invalid_argument("malformed epoch value");
            }
            return from_epoch_float(v);
        }
        std::int64_t v = 0;
        const auto res = std::from_chars(digits.data(), digits.data() + digits.size(), v);
        if (res.ec == std::errc::result_out_of_range) {
            throw std::out_of_range("epoch value out of range");
        }
        if (res.ec != std::errc{} || res.ptr != digits.data() + digits.size()) {
            throw std::invalid_argument("malformed epoch value");
        }
        return from_epoch_integer(v);
    }
    // RFC 2822 has a weekday name and comma in the fourth position ("Sun,").
    if (s.find(',') == 3) {
        return parse_rfc2822(s);
    }
    return parse_iso(s);
}

}  // namespace cppref::chrono_time
