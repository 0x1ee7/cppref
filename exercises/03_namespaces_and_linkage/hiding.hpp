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
    // "\"<text>\"" — no escaping.
    // Note: this declaration hides Formatter::format(int). Making q.format(42) compile
    // requires a using-declaration (see README, Part 4).
    std::string format(std::string_view text) const;
};

// True if T has a callable format(int) member (checked in a template context).
template <typename T>
concept CanFormatInt = requires(const T& f) { f.format(42); };

}  // namespace cppref::hiding
