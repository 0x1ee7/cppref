#include "form_arena.hpp"

#include <stdexcept>

namespace cppref::performance {

std::expected<std::pmr::vector<FormField>, FormParseError> parse_form(
    std::string_view /*body*/, std::pmr::memory_resource* /*arena*/) {
    throw std::logic_error("not implemented: parse_form");
}

}  // namespace cppref::performance
