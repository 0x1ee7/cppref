#include "config.hpp"

#include "expected_utils.hpp"

#include <cctype>
#include <charconv>
#include <cstdint>
#include <expected>
#include <format>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace cppref::functional {

namespace {

std::string_view trim(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }
    return text;
}

bool is_known_key(std::string_view key) {
    return key == "host" || key == "port" || key == "debug" || key == "workers";
}

}  // namespace

std::expected<std::uint16_t, std::string> parse_port(std::string_view raw) {
    const std::string_view text = trim(raw);
    long long value = 0;
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec == std::errc::result_out_of_range) {
        return std::unexpected("port must be between 1 and 65535");
    }
    if (text.empty() || ec != std::errc{} || ptr != text.data() + text.size()) {
        return std::unexpected(std::format("port must be an integer, got '{}'", text));
    }
    if (value < 1 || value > 65535) {
        return std::unexpected("port must be between 1 and 65535");
    }
    return static_cast<std::uint16_t>(value);
}

std::expected<std::vector<std::uint16_t>, std::string> parse_ports(std::string_view raw) {
    std::vector<std::expected<std::uint16_t, std::string>> parsed;
    for (std::size_t start = 0;;) {
        const std::size_t comma = raw.find(',', start);
        const std::size_t length = comma == std::string_view::npos ? std::string_view::npos : comma - start;
        parsed.push_back(parse_port(raw.substr(start, length)));
        if (comma == std::string_view::npos) {
            break;
        }
        start = comma + 1;
    }
    return collect(parsed);
}

std::expected<bool, std::string> parse_bool(std::string_view raw) {
    const std::string_view text = trim(raw);
    std::string lower;
    lower.reserve(text.size());
    for (char c : text) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    if (lower == "true" || lower == "yes" || lower == "1" || lower == "on") {
        return true;
    }
    if (lower == "false" || lower == "no" || lower == "0" || lower == "off") {
        return false;
    }
    return std::unexpected(std::format("expected boolean, got '{}'", text));
}

std::expected<Config, std::vector<std::string>> parse_config(std::string_view text) {
    Config config;
    std::vector<std::string> errors;
    std::set<std::string_view> seen;
    bool has_host = false;
    bool has_port = false;

    std::size_t line_no = 0;
    for (std::size_t start = 0;;) {
        const std::size_t newline = text.find('\n', start);
        const std::size_t length = newline == std::string_view::npos ? std::string_view::npos : newline - start;
        const std::string_view line = text.substr(start, length);
        ++line_no;

        const std::string_view stripped = trim(line);
        if (!stripped.empty() && stripped.front() != '#') {
            const std::size_t eq = stripped.find('=');
            if (eq == std::string_view::npos) {
                errors.push_back(std::format("line {}: expected 'key = value'", line_no));
            } else {
                const std::string_view key = trim(stripped.substr(0, eq));
                const std::string_view value = trim(stripped.substr(eq + 1));
                if (!is_known_key(key)) {
                    errors.push_back(std::format("line {}: unknown key '{}'", line_no, key));
                } else if (!seen.insert(key).second) {
                    errors.push_back(std::format("line {}: duplicate key '{}'", line_no, key));
                } else if (key == "host") {
                    has_host = true;
                    if (value.empty()) {
                        errors.push_back(std::format("line {}: host must not be empty", line_no));
                    } else {
                        config.host = std::string(value);
                    }
                } else if (key == "port") {
                    has_port = true;
                    if (auto port = parse_port(value)) {
                        config.port = *port;
                    } else {
                        errors.push_back(std::format("line {}: {}", line_no, port.error()));
                    }
                } else if (key == "debug") {
                    if (auto flag = parse_bool(value)) {
                        config.debug = *flag;
                    } else {
                        errors.push_back(std::format("line {}: {}", line_no, flag.error()));
                    }
                } else {  // workers
                    long long workers = 0;
                    const auto [ptr, ec] =
                        std::from_chars(value.data(), value.data() + value.size(), workers);
                    if (ec == std::errc{} && ptr == value.data() + value.size() && workers >= 1 && workers <= 64) {
                        config.workers = static_cast<int>(workers);
                    } else {
                        errors.push_back(std::format("line {}: workers must be between 1 and 64", line_no));
                    }
                }
            }
        }

        if (newline == std::string_view::npos) {
            break;
        }
        start = newline + 1;
    }

    if (!has_host) {
        errors.emplace_back("missing required key 'host'");
    }
    if (!has_port) {
        errors.emplace_back("missing required key 'port'");
    }

    if (!errors.empty()) {
        return std::unexpected(std::move(errors));
    }
    return config;
}

}  // namespace cppref::functional
