// Part 6: class-type non-type template parameters, CTAD and consteval checks.
#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace cppref::templates_and_concepts {

/// String literal usable as an NTTP. `N` counts the terminating NUL.
template <std::size_t N>
struct FixedString {
  char data[N]{};

  constexpr FixedString(const char (&text)[N]) noexcept {
    for (std::size_t i = 0; i < N; ++i) {
      data[i] = text[i];
    }
  }

  constexpr std::string_view view() const noexcept {
    return std::string_view{data, N - 1};
  }
};

/// Well-formed route: leading '/', no empty segments (a trailing '/' is
/// empty), `{name}` is a whole segment with a non-empty name, nothing after a
/// closing brace within its segment, no stray braces. "/" alone is valid.
constexpr bool route_is_valid(std::string_view pattern) noexcept {
  if (pattern.empty() || pattern.front() != '/') return false;
  if (pattern == "/") return true;

  bool in_param = false;
  bool closed_param = false;
  std::size_t seg_len = 0;
  std::size_t name_len = 0;
  for (std::size_t i = 1; i <= pattern.size(); ++i) {
    // The sentinel '/' at i == size closes the final segment.
    const char c = i < pattern.size() ? pattern[i] : '/';
    if (in_param) {
      if (c == '}') {
        if (name_len == 0) return false;
        in_param = false;
        closed_param = true;
      } else if (c == '{' || c == '/') {
        return false;
      } else {
        ++name_len;
      }
      continue;
    }
    if (c == '/') {
      if (seg_len == 0) return false;
      seg_len = 0;
      closed_param = false;
      continue;
    }
    if (closed_param) return false;
    if (c == '{') {
      if (seg_len != 0) return false;
      in_param = true;
      name_len = 0;
      ++seg_len;
      continue;
    }
    if (c == '}') return false;
    ++seg_len;
  }
  return !in_param;
}

/// Number of `{name}` parameters. An invalid pattern is a compile-time error
/// when this is used in a constant expression.
consteval std::size_t checked_param_count(std::string_view pattern) {
  if (!route_is_valid(pattern)) {
    throw std::invalid_argument("invalid route pattern");
  }
  std::size_t count = 0;
  for (char c : pattern) {
    if (c == '{') ++count;
  }
  return count;
}

/// Route pattern checked at compile time, with a runtime matcher.
template <FixedString Pattern>
struct Route {
  static constexpr std::string_view pattern = Pattern.view();
  static constexpr bool is_valid = route_is_valid(Pattern.view());
  static constexpr std::size_t param_count =
      checked_param_count(Pattern.view());

  /// Segment-by-segment match. A `{name}` segment matches any non-empty
  /// segment; literal segments must be equal; segment counts must agree.
  static bool matches(std::string_view path) {
    std::string_view pat = pattern;
    if (pat == "/") return path == "/";
    if (path.empty() || path.front() != '/') return false;
    pat.remove_prefix(1);
    path.remove_prefix(1);
    while (true) {
      const auto pat_slash = pat.find('/');
      const auto path_slash = path.find('/');
      const std::string_view pat_seg = pat.substr(0, pat_slash);
      const std::string_view path_seg = path.substr(0, path_slash);
      const bool is_param =
          pat_seg.size() > 2 && pat_seg.front() == '{' && pat_seg.back() == '}';
      if (is_param ? path_seg.empty() : pat_seg != path_seg) return false;
      const bool pat_done = pat_slash == std::string_view::npos;
      const bool path_done = path_slash == std::string_view::npos;
      if (pat_done || path_done) return pat_done && path_done;
      pat.remove_prefix(pat_slash + 1);
      path.remove_prefix(path_slash + 1);
    }
  }
};

/// Value with a label; CTAD picks the element type from the argument.
template <typename T>
class Labeled {
 public:
  explicit Labeled(T value) : value_(std::move(value)) {}

  const T& value() const noexcept { return value_; }

 private:
  T value_{};
};

/// A string literal is stored as std::string, not const char*.
Labeled(const char*) -> Labeled<std::string>;

}  // namespace cppref::templates_and_concepts
