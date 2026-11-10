#include "adl.hpp"

#include <format>
#include <string>
#include <utility>

namespace shapes {

void swap(Point& a, Point& b) {
    std::swap(a.x, b.x);
    std::swap(a.y, b.y);
}

std::string to_string(const Point& p) {
    return std::format("({}, {})", p.x, p.y);
}

}  // namespace shapes
