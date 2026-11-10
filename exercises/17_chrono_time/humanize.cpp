#include "humanize.hpp"

#include <stdexcept>

namespace cppref::chrono_time {

std::string humanize(const ZonedInstant&, const ZonedInstant&) {
    throw std::logic_error("not implemented: humanize");
}

}  // namespace cppref::chrono_time
