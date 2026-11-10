#include "formatting.hpp"

#include <stdexcept>

namespace cppref::performance {

std::string join_csv(std::span<const std::string_view> /*fields*/) {
    throw std::logic_error("not implemented: join_csv");
}

std::string format_access_line(const AccessEntry& /*entry*/) {
    throw std::logic_error("not implemented: format_access_line");
}

std::expected<std::string_view, std::size_t> format_access_line_into(std::span<char> /*out*/,
                                                                     const AccessEntry& /*entry*/) {
    throw std::logic_error("not implemented: format_access_line_into");
}

}  // namespace cppref::performance
