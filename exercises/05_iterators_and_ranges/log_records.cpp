// Engineer-owned. Definitions for log_records.hpp (templates live in the header).
#include "log_records.hpp"

#include <stdexcept>

namespace cppref::iterators_and_ranges {

std::optional<Level> parse_level(std::string_view) {
    throw std::logic_error("not implemented: parse_level");
}

LogParseError::LogParseError(std::string reason, std::size_t)
    : std::runtime_error(reason) {
    throw std::logic_error("not implemented: LogParseError");
}

std::size_t LogParseError::lineno() const {
    throw std::logic_error("not implemented: LogParseError::lineno");
}

const std::string& LogParseError::reason() const {
    throw std::logic_error("not implemented: LogParseError::reason");
}

std::expected<LogRecord, LogParseError> parse_line(std::string_view) {
    throw std::logic_error("not implemented: parse_line");
}

AtLeast::AtLeast(Level) {
    throw std::logic_error("not implemented: AtLeast");
}

bool AtLeast::operator()(const LogRecord&) const {
    throw std::logic_error("not implemented: AtLeast::operator()");
}

}  // namespace cppref::iterators_and_ranges
