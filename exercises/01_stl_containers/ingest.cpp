#include "ingest.hpp"

#include <stdexcept>

namespace cppref::stl {

std::size_t drop_invalid(std::vector<Reading>&) {
    throw std::logic_error("not implemented: drop_invalid");
}

std::vector<Reading> select_sensor(std::span<const Reading>, std::string_view) {
    throw std::logic_error("not implemented: select_sensor");
}

std::vector<std::string_view> split_fields(std::string_view, char) {
    throw std::logic_error("not implemented: split_fields");
}

std::optional<std::string_view> hottest_sensor(std::span<const Reading>) {
    throw std::logic_error("not implemented: hottest_sensor");
}

}  // namespace cppref::stl
