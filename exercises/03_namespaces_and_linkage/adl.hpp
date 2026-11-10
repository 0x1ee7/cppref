#pragma once

#include <concepts>
#include <stdexcept>
#include <string>
#include <utility>

// Part 4a — argument-dependent lookup (ADL), customisation points and niebloids.
namespace shapes {

struct Point {
    double x = 0;
    double y = 0;
};

// Found by ADL. Swaps the coordinates of a and b.
void swap(Point& a, Point& b);

// Found by ADL. "(x, y)" using std::format's default double formatting, e.g. "(1, 2)".
std::string to_string(const Point& p);

}  // namespace shapes

namespace adl {

// A type is Textual when an unqualified call to_string(t) (found via ADL) returns something
// convertible to std::string. int is not Textual: no to_string is visible for it.
template <typename T>
concept Textual = requires(const T& t) {
    { to_string(t) } -> std::convertible_to<std::string>;
};

// Niebloid: calls swap(a, b) via ADL so a type's own swap wins, and falls back to std::swap.
struct SwapValues {
    template <typename T>
    void operator()(T& a, T& b) const {
        (void)a;
        (void)b;
        throw std::logic_error("not implemented: adl::swap_values");
    }
};
inline constexpr SwapValues swap_values{};

// Niebloid: returns to_string(value) via ADL. Constrained to Textual types.
struct TextOf {
    template <Textual T>
    std::string operator()(const T& value) const {
        (void)value;
        throw std::logic_error("not implemented: adl::text_of");
    }
};
inline constexpr TextOf text_of{};

}  // namespace adl
