#include "log_parser.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>
#include <system_error>
#include <utility>

namespace cppref::performance {
namespace {

constexpr std::size_t kNpos = std::string_view::npos;
constexpr std::string_view kStatusKey = "status=";
constexpr std::string_view kLatencyKey = "latency_us=";

using Result = std::expected<LogLine, ParseError>;

struct Token {
    std::string_view text;
    std::size_t start;  // offset of the token; line.size() when absent
};

// Next space-separated token at `pos`; advances `pos` past the separator.
Token next_token(std::string_view line, std::size_t& pos) {
    const std::size_t start = pos;
    const std::size_t space = line.find(' ', start);
    if (space == kNpos) {
        pos = line.size();
        return {line.substr(start), start};
    }
    pos = space + 1;
    return {line.substr(start, space - start), start};
}

std::unexpected<ParseError> fail(ParseErrorKind kind, std::size_t column) {
    return std::unexpected(ParseError{kind, column});
}

// Strict unsigned decimal: non-empty, digits only, in range for T.
template <class T>
bool parse_unsigned(std::string_view text, T& out) {
    if (text.empty()) return false;
    const char* last = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(text.data(), last, out);
    return ec == std::errc{} && ptr == last;
}

bool level_from(std::string_view text, LogLevel& out) {
    if (text == "TRACE") out = LogLevel::trace;
    else if (text == "DEBUG") out = LogLevel::debug;
    else if (text == "INFO") out = LogLevel::info;
    else if (text == "WARN") out = LogLevel::warn;
    else if (text == "ERROR") out = LogLevel::error;
    else return false;
    return true;
}

}  // namespace

Result parse_line(std::string_view line) {
    std::size_t pos = 0;

    const Token timestamp = next_token(line, pos);
    if (timestamp.text.empty()) return fail(ParseErrorKind::missing_field, timestamp.start);

    const Token level_tok = next_token(line, pos);
    if (level_tok.text.empty()) return fail(ParseErrorKind::missing_field, level_tok.start);
    LogLevel level{};
    if (!level_from(level_tok.text, level)) return fail(ParseErrorKind::bad_level, level_tok.start);

    const Token component = next_token(line, pos);
    if (component.text.empty()) return fail(ParseErrorKind::missing_field, component.start);

    const Token status_tok = next_token(line, pos);
    if (!status_tok.text.starts_with(kStatusKey)) {
        return fail(ParseErrorKind::missing_field, status_tok.start);
    }
    std::uint32_t status = 0;
    if (!parse_unsigned(status_tok.text.substr(kStatusKey.size()), status)) {
        return fail(ParseErrorKind::bad_number, status_tok.start + kStatusKey.size());
    }

    const Token latency_tok = next_token(line, pos);
    if (!latency_tok.text.starts_with(kLatencyKey)) {
        return fail(ParseErrorKind::missing_field, latency_tok.start);
    }
    std::uint64_t latency = 0;
    if (!parse_unsigned(latency_tok.text.substr(kLatencyKey.size()), latency)) {
        return fail(ParseErrorKind::bad_number, latency_tok.start + kLatencyKey.size());
    }

    // `pos` is past the separator after latency_us; an absent message is empty.
    const std::string_view message = pos < line.size() ? line.substr(pos) : std::string_view{};
    return LogLine{timestamp.text, level, component.text, status, latency, message};
}

std::string_view to_string(LogLevel level) {
    switch (level) {
        case LogLevel::trace: return "TRACE";
        case LogLevel::debug: return "DEBUG";
        case LogLevel::info: return "INFO";
        case LogLevel::warn: return "WARN";
        case LogLevel::error: return "ERROR";
    }
    return "";
}

LevelCounts count_levels(std::string_view log) {
    LevelCounts counts;
    while (!log.empty()) {
        const std::size_t newline = log.find('\n');
        std::string_view line = log.substr(0, newline);
        log = newline == kNpos ? std::string_view{} : log.substr(newline + 1);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.empty()) continue;

        if (const auto parsed = parse_line(line)) {
            ++counts.by_level[static_cast<std::size_t>(parsed->level)];
        } else {
            ++counts.malformed;
        }
    }
    return counts;
}

}  // namespace cppref::performance
