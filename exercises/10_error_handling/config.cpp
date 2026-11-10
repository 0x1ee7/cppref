#include "config.hpp"

#include <stdexcept>

namespace cppref::error_handling {

ParseError::ParseError(std::size_t /*line*/, const std::string& /*detail*/) : std::runtime_error("") {
    throw std::logic_error("not implemented: ParseError::ParseError");
}

std::size_t ParseError::line() const {
    throw std::logic_error("not implemented: ParseError::line");
}

Config parse_config(std::string_view /*text*/) {
    throw std::logic_error("not implemented: parse_config");
}

Config load_config(const std::filesystem::path& /*path*/) {
    throw std::logic_error("not implemented: load_config");
}

void throw_invalid_setting(std::string_view /*key*/, std::string_view /*raw*/) {
    throw std::logic_error("not implemented: throw_invalid_setting");
}

std::expected<Config, ConfigFailure> try_load_config(const std::filesystem::path& /*path*/) {
    throw std::logic_error("not implemented: try_load_config");
}

Config value_or_throw(std::expected<Config, ConfigFailure> /*result*/, const std::filesystem::path& /*path*/) {
    throw std::logic_error("not implemented: value_or_throw");
}

}  // namespace cppref::error_handling
