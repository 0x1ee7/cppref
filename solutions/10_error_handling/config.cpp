#include "config.hpp"

#include <cstddef>
#include <exception>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace cppref::error_handling {

namespace fs = std::filesystem;

namespace {

constexpr std::string_view kWhitespace = " \t\r";

std::string_view trim(std::string_view s) {
    const auto first = s.find_first_not_of(kWhitespace);
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = s.find_last_not_of(kWhitespace);
    return s.substr(first, last - first + 1);
}

// Throws Wrapper(message) with a std::system_error(code, detail) nested under it.
template <typename Wrapper>
[[noreturn]] void throw_nested_system(const std::string& message, std::errc code, const std::string& detail) {
    try {
        throw std::system_error(std::make_error_code(code), detail);
    } catch (...) {
        std::throw_with_nested(Wrapper(message));
    }
}

}  // namespace

ParseError::ParseError(std::size_t line, const std::string& detail)
    : std::runtime_error("line " + std::to_string(line) + ": " + detail), line_(line) {}

std::size_t ParseError::line() const {
    return line_;
}

Config parse_config(std::string_view text) {
    Config cfg;
    std::string section;
    std::size_t line_no = 0;

    while (!text.empty()) {
        const auto newline = text.find('\n');
        const std::string_view raw = text.substr(0, newline);
        text = newline == std::string_view::npos ? std::string_view{} : text.substr(newline + 1);
        ++line_no;

        const std::string_view line = trim(raw);
        if (line.empty() || line.front() == '#') {
            continue;
        }
        if (line.front() == '[') {
            if (line.size() < 2 || line.back() != ']') {
                throw ParseError(line_no, "unterminated section header");
            }
            const std::string_view name = trim(line.substr(1, line.size() - 2));
            if (name.empty()) {
                throw ParseError(line_no, "empty section name");
            }
            section = std::string(name);
            continue;
        }

        const auto eq = line.find('=');
        if (eq == std::string_view::npos) {
            throw ParseError(line_no, "expected 'key = value'");
        }
        const std::string_view key = trim(line.substr(0, eq));
        if (key.empty()) {
            throw ParseError(line_no, "empty key");
        }
        const std::string_view value = trim(line.substr(eq + 1));

        std::string full = section.empty() ? std::string(key) : section + "." + std::string(key);
        auto [it, inserted] = cfg.try_emplace(full, std::string(value));
        if (!inserted) {
            throw ParseError(line_no, "duplicate key '" + full + "'");
        }
    }
    return cfg;
}

Config load_config(const fs::path& path) {
    std::error_code ec;
    if (!fs::exists(path, ec) || ec) {
        throw_nested_system<ConfigNotFound>("config file not found: " + path.string(),
                                            std::errc::no_such_file_or_directory, path.string());
    }
    if (fs::is_directory(path, ec)) {
        throw_nested_system<ConfigUnreadable>("cannot read config: " + path.string(),
                                              std::errc::is_a_directory, path.string());
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw_nested_system<ConfigUnreadable>("cannot read config: " + path.string(),
                                              std::errc::io_error, path.string());
    }
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    try {
        return parse_config(text);
    } catch (const ParseError& e) {
        std::throw_with_nested(ConfigInvalid(path.string() + ": " + e.what()));
    }
}

void throw_invalid_setting(std::string_view key, std::string_view raw) {
    try {
        throw std::invalid_argument("'" + std::string(raw) + "' is not a valid integer");
    } catch (...) {
        std::throw_with_nested(ConfigInvalid("setting '" + std::string(key) + "' has invalid value '" +
                                             std::string(raw) + "'"));
    }
}

std::expected<Config, ConfigFailure> try_load_config(const fs::path& path) {
    try {
        return load_config(path);
    } catch (const ConfigNotFound&) {
        return std::unexpected(ConfigFailure::not_found);
    } catch (const ConfigUnreadable&) {
        return std::unexpected(ConfigFailure::unreadable);
    } catch (const ConfigInvalid&) {
        return std::unexpected(ConfigFailure::invalid);
    }
}

Config value_or_throw(std::expected<Config, ConfigFailure> result, const fs::path& path) {
    if (result) {
        return std::move(*result);
    }
    const std::string where = path.string();
    switch (result.error()) {
        case ConfigFailure::not_found:
            throw ConfigNotFound("config file not found: " + where);
        case ConfigFailure::unreadable:
            throw ConfigUnreadable("cannot read config: " + where);
        case ConfigFailure::invalid:
            throw ConfigInvalid(where + ": invalid config");
    }
    throw std::logic_error("value_or_throw: unknown ConfigFailure");
}

}  // namespace cppref::error_handling
