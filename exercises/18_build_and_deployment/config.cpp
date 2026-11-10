#include "config.hpp"

#include <stdexcept>

namespace cppref::deployment {

std::expected<LogLevel, std::string> parse_log_level(std::string_view) {
    throw std::logic_error("not implemented: parse_log_level");
}

std::string_view to_string(LogLevel) {
    throw std::logic_error("not implemented: to_string(LogLevel)");
}

ConfigError::ConfigError(std::vector<std::string>) : std::runtime_error("") {
    throw std::logic_error("not implemented: ConfigError");
}

const std::vector<std::string>& ConfigError::errors() const {
    throw std::logic_error("not implemented: ConfigError::errors");
}

Config Config::from_env(const EnvLookup&) {
    throw std::logic_error("not implemented: Config::from_env");
}

std::string redacted(const Config&) {
    throw std::logic_error("not implemented: redacted");
}

std::expected<bool, std::string> parse_bool(std::string_view) {
    throw std::logic_error("not implemented: parse_bool");
}

std::expected<std::chrono::milliseconds, std::string> parse_duration(std::string_view) {
    throw std::logic_error("not implemented: parse_duration");
}

std::vector<std::string> parse_list(std::string_view) {
    throw std::logic_error("not implemented: parse_list");
}

std::string mask_url_password(std::string_view) {
    throw std::logic_error("not implemented: mask_url_password");
}

std::expected<DotenvMap, std::string> load_dotenv(std::string_view) {
    throw std::logic_error("not implemented: load_dotenv");
}

EnvLookup with_dotenv(EnvLookup, DotenvMap) {
    throw std::logic_error("not implemented: with_dotenv");
}

}  // namespace cppref::deployment
