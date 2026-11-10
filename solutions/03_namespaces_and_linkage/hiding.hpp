#pragma once

#include <concepts>
#include <string>
#include <string_view>

// Part 4b — name hiding: a derived declaration hides every base overload of the same name.
namespace cppref::hiding {

class Formatter {
public:
    virtual ~Formatter() = default;

    // "#<value>", e.g. "#42".
    std::string format(int value) const;
};

class QuotedFormatter : public Formatter {
public:
    // Brings Formatter::format(int) back into scope; without it, the declaration below
    // hides the base overload.
    using Formatter::format;

    // "\"<text>\"" — no escaping.
    std::string format(std::string_view text) const;
};

// True if T has a callable format(int) member (checked in a template context).
template <typename T>
concept CanFormatInt = requires(const T& f) { f.format(42); };

}  // namespace cppref::hiding
