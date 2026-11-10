// Part 3: variadic templates, fold expressions, if constexpr, type traits and
// std::type_identity.
#pragma once

#include <charconv>
#include <concepts>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>

namespace cppref::templates_and_concepts {

namespace detail {

template <typename>
inline constexpr bool dependent_false_v = false;

}  // namespace detail

/// Arithmetic types, and anything convertible to std::string_view.
template <typename T>
concept TextValue = std::is_arithmetic_v<std::remove_cvref_t<T>> ||
                    std::convertible_to<const T&, std::string_view>;

/// Sum of integral arguments. Accumulates in the common type, so small
/// integers are not promoted implicitly beyond it.
template <std::integral... Ts>
  requires(sizeof...(Ts) > 0)
std::common_type_t<Ts...> sum_all(Ts... values) {
  using R = std::common_type_t<Ts...>;
  return static_cast<R>((static_cast<R>(values) + ...));
}

/// Occurrences of T in Ts..., via a fold over per-element comparisons.
template <typename T, typename... Ts>
inline constexpr std::size_t count_of_v =
    (std::size_t{0} + ... +
     (std::is_same_v<T, Ts> ? std::size_t{1} : std::size_t{0}));

/// Text of one value. `if constexpr` picks the branch at compile time.
template <TextValue T>
std::string to_text(const T& value) {
  using U = std::remove_cvref_t<T>;
  if constexpr (std::same_as<U, bool>) {
    return value ? "true" : "false";
  } else if constexpr (std::is_arithmetic_v<U>) {
    return std::to_string(value);
  } else {
    return std::string{std::string_view{value}};
  }
}

/// Joins `to_text` of each argument with `separator`. Empty pack gives "".
template <TextValue... Ts>
std::string join_text(std::string_view separator, const Ts&... parts) {
  std::string out;
  bool first = true;
  auto append = [&](const std::string& piece) {
    if (!first) {
      out.append(separator);
    }
    first = false;
    out += piece;
  };
  (append(to_text(parts)), ...);
  return out;
}

/// Category name used for dispatch.
template <typename T>
constexpr std::string_view kind_name() noexcept {
  using U = std::remove_cvref_t<T>;
  if constexpr (std::same_as<U, bool>) {
    return "boolean";
  } else if constexpr (std::integral<U>) {
    return "integer";
  } else if constexpr (std::floating_point<U>) {
    return "floating";
  } else if constexpr (std::convertible_to<const U&, std::string_view>) {
    return "text";
  } else {
    return "other";
  }
}

/// Parses `text` as T; falls back when parsing fails. Integral and floating
/// types must consume the entire text. `type_identity_t` keeps T non-deduced
/// from the fallback argument.
template <typename T>
T parse_or(std::string_view text, std::type_identity_t<T> fallback) {
  if constexpr (std::same_as<T, bool>) {
    if (text == "true") return true;
    if (text == "false") return false;
    return fallback;
  } else if constexpr (std::integral<T> || std::floating_point<T>) {
    T value{};
    const char* first = text.data();
    const char* last = first + text.size();
    auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec == std::errc{} && ptr == last) {
      return value;
    }
    return fallback;
  } else if constexpr (std::same_as<T, std::string>) {
    return std::string{text};
  } else {
    static_assert(detail::dependent_false_v<T>,
                  "parse_or: unsupported target type");
  }
}

}  // namespace cppref::templates_and_concepts
