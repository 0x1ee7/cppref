#include "adl.hpp"

#include <stdexcept>

namespace shapes {

void swap(Point&, Point&) {
    throw std::logic_error("not implemented: shapes::swap");
}

std::string to_string(const Point&) {
    throw std::logic_error("not implemented: shapes::to_string");
}

}  // namespace shapes
