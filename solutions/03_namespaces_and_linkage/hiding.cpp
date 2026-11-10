#include "hiding.hpp"

#include <format>
#include <string>
#include <string_view>

namespace cppref::hiding {

std::string Formatter::format(int value) const {
    return std::format("#{}", value);
}

std::string QuotedFormatter::format(std::string_view text) const {
    return std::format("\"{}\"", text);
}

}  // namespace cppref::hiding
