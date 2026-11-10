// Part 6: class-type non-type template parameters, CTAD and consteval checks.
#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace cppref::templates_and_concepts {

/// A string literal usable as a non-type template parameter.
/// `N` counts the terminating NUL. Used as `Route<"/users/{id}">`.
///
/// STUBS: the constructor and view() are neutral (no throw) so that
/// compile-time instantiation still succeeds; tests then fail on values.
template <std::size_t N>
struct FixedString {
  char data[N]{};

  constexpr FixedString(const char (&text)[N]) noexcept {
    (void)text;  // Engineer: copy `text` into `data`.
  }

  /// The text without the terminating NUL. STUB: empty view.
  constexpr std::string_view view() const noexcept { return {}; }
};

/// True when `pattern` is a well-formed route: starts with '/', each segment
/// is non-empty, `{name}` segments have a non-empty name and are closed, and
/// there are no stray braces.
///
/// STUB: returns false (neutral; keeps compile-time evaluation legal).
constexpr bool route_is_valid(std::string_view pattern) noexcept {
  (void)pattern;
  return false;
}

/// Number of `{name}` parameters in a valid pattern. consteval: an invalid
/// pattern is a compile-time error (the engineer's solution throws here).
///
/// STUB: returns 0.
consteval std::size_t checked_param_count(std::string_view pattern) {
  (void)pattern;
  return 0;
}

/// A route pattern checked at compile time.
template <FixedString Pattern>
struct Route {
  /// The pattern text.
  static constexpr std::string_view pattern = Pattern.view();

  /// Whether the pattern is well-formed (usable as a runtime check).
  static constexpr bool is_valid = route_is_valid(Pattern.view());

  /// Number of parameters. Invalid patterns fail to compile.
  static constexpr std::size_t param_count =
      checked_param_count(Pattern.view());

  /// True when `path` matches the pattern: same number of segments, literal
  /// segments equal, `{name}` segments match any non-empty segment.
  static bool matches(std::string_view path) {
    (void)path;
    throw std::logic_error("not implemented: Route::matches");
  }
};

/// A value tagged with a human-readable label. CTAD: `Labeled{"hi"}` deduces
/// `Labeled<std::string>` (see the deduction guide), `Labeled{42}` deduces
/// `Labeled<int>`.
///
/// STUB: the constructor throws; the engineer stores `T value_` (declared
/// below).
template <typename T>
class Labeled {
 public:
  explicit Labeled(T value) : value_(std::move(value)) {
    throw std::logic_error("not implemented: Labeled::Labeled");
  }

  /// STUB: throws.
  const T& value() const {
    throw std::logic_error("not implemented: Labeled::value");
  }

 private:
  T value_{};
};

/// Deduction guide: a string literal stores a std::string.
Labeled(const char*) -> Labeled<std::string>;

}  // namespace cppref::templates_and_concepts
