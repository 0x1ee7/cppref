#include "parse.hpp"

#include <stdexcept>

namespace cppref::chrono_time {

Instant parse_timestamp(std::string_view) {
    throw std::logic_error("not implemented: parse_timestamp");
}

Instant from_epoch_integer(std::int64_t) {
    throw std::logic_error("not implemented: from_epoch_integer");
}

Instant from_epoch_float(double) {
    throw std::logic_error("not implemented: from_epoch_float");
}

}  // namespace cppref::chrono_time
