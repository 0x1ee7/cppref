#include "amounts.hpp"

#include <stdexcept>

namespace cppref::error_handling {

std::expected<Money, ParseFailure> parse_amount(std::string_view /*text*/) {
    throw std::logic_error("not implemented: parse_amount");
}

std::expected<int, ParseFailure> parse_quantity(std::string_view /*text*/) {
    throw std::logic_error("not implemented: parse_quantity");
}

std::expected<LineItem, LineItemError> parse_line_item(std::string_view /*line*/) {
    throw std::logic_error("not implemented: parse_line_item");
}

}  // namespace cppref::error_handling
