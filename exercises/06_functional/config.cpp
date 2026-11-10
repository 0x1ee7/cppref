#include "config.hpp"

#include <stdexcept>

// Part 5 · Config parsing. Stub: implement every function.

namespace cppref::functional {

std::expected<std::uint16_t, std::string> parse_port(std::string_view) {
    throw std::logic_error("not implemented: parse_port");
}

std::expected<std::vector<std::uint16_t>, std::string> parse_ports(std::string_view) {
    throw std::logic_error("not implemented: parse_ports");
}

std::expected<bool, std::string> parse_bool(std::string_view) {
    throw std::logic_error("not implemented: parse_bool");
}

std::expected<Config, std::vector<std::string>> parse_config(std::string_view) {
    throw std::logic_error("not implemented: parse_config");
}

}  // namespace cppref::functional
