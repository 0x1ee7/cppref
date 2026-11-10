#include "log_parser.hpp"

#include <stdexcept>

namespace cppref::performance {

std::expected<LogLine, ParseError> parse_line(std::string_view /*line*/) {
    throw std::logic_error("not implemented: parse_line");
}

std::string_view to_string(LogLevel /*level*/) {
    throw std::logic_error("not implemented: to_string");
}

LevelCounts count_levels(std::string_view /*log*/) {
    throw std::logic_error("not implemented: count_levels");
}

}  // namespace cppref::performance
