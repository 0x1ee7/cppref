// Part 3: variadic templates, fold expressions, if constexpr, type traits and
// std::type_identity.
#pragma once

#include <concepts>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace cppref::templates_and_concepts {

/// Types that `to_text` and `join_text` accept: arithmetic types, and types
/// convertible to `std::string_view` (strings, literals, views).
template <typename T>
concept TextValue = std::is_arithmetic_v<std::remove_cvref_t<T>> ||
                    std::convertible_to<const T&, std::string_view>;

/// Sum of one or more integral arguments. The result type is the common type
/// of the arguments.
template <std::integral... Ts>
  requires(sizeof...(Ts) > 0)
std::common_type_t<Ts...> sum_all(Ts... values) {
  (void)sizeof...(values);
  throw std::logic_error("not implemented: sum_all");
}

/// Number of occurrences of `T` in `Ts...`.
///
/// STUB: value is 0; the engineer computes it with a fold expression.
template <typename T, typename... Ts>
inline constexpr std::size_t count_of_v = 0;

/// Text of one value: `bool` gives "true"/"false"; string-like values are
/// returned as-is; other arithmetic values are formatted with std::to_string
/// or an equivalent. Uses `if constexpr` to choose the branch.
template <TextValue T>
std::string to_text(const T& value) {
  (void)value;
  throw std::logic_error("not implemented: to_text");
}

/// Concatenation of `to_text` of each argument, separated by `separator`.
/// Empty pack gives "".
template <TextValue... Ts>
std::string join_text(std::string_view separator, const Ts&... parts) {
  (void)separator;
  (void)sizeof...(parts);
  throw std::logic_error("not implemented: join_text");
}

/// Name of the category of T: "boolean", "integer", "floating", "text" or
/// "other". Dispatch uses `if constexpr` and type traits.
///
/// STUB: returns "" (never throws; constexpr).
template <typename T>
constexpr std::string_view kind_name() noexcept {
  return {};
}

/// Parses `text` as T and returns the result, or `fallback` when parsing
/// fails. Supported: integral types (whole text must be consumed), floating
/// point types, bool ("true"/"false" exactly), and std::string (always
/// succeeds with a copy of the text).
///
/// `std::type_identity_t<T>` keeps T non-deduced from `fallback`, so
/// `parse_or<double>("x", 2)` is valid.
template <typename T>
T parse_or(std::string_view text, std::type_identity_t<T> fallback) {
  (void)text;
  (void)fallback;
  throw std::logic_error("not implemented: parse_or");
}

}  // namespace cppref::templates_and_concepts
