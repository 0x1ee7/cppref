#include "layered_config.hpp"

#include <stdexcept>

namespace cppref::stl {

Value deep_merge(const Value&, const Value&) {
    throw std::logic_error("not implemented: deep_merge");
}

Table parse_env(const std::map<std::string, std::string>&, std::string_view) {
    throw std::logic_error("not implemented: parse_env");
}

Config::Config(const ConfigLayers&) {
    throw std::logic_error("not implemented: Config::Config");
}

const Value* Config::get(std::string_view) const {
    throw std::logic_error("not implemented: Config::get");
}

std::optional<Layer> Config::source(std::string_view) const {
    throw std::logic_error("not implemented: Config::source");
}

Table Config::as_dict() const {
    throw std::logic_error("not implemented: Config::as_dict");
}

}  // namespace cppref::stl
