#include "hiding.hpp"

#include <stdexcept>

namespace cppref::hiding {

std::string Formatter::format(int) const {
    throw std::logic_error("not implemented: Formatter::format");
}

std::string QuotedFormatter::format(std::string_view) const {
    throw std::logic_error("not implemented: QuotedFormatter::format");
}

}  // namespace cppref::hiding
