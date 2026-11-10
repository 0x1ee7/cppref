// Part 1: concepts, requires-clauses and subsumption.
#pragma once

#include <concepts>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>

namespace cppref::templates_and_concepts {

namespace detail {

/// Formats any streamable value for diagnostics and descriptions.
template <typename T>
std::string to_display(const T& value) {
  std::ostringstream out;
  out << value;
  return out.str();
}

}  // namespace detail

/// An entity has an ordered `id_type` and a const `id()` returning exactly it.
template <typename T>
concept Entity = requires(const T& t) {
  typename T::id_type;
  requires std::totally_ordered<typename T::id_type>;
  { t.id() } -> std::same_as<typename T::id_type>;
};

/// A named type has a const `name()` convertible to `std::string_view`.
template <typename T>
concept Named = requires(const T& t) {
  { t.name() } -> std::convertible_to<std::string_view>;
};

/// Unconstrained fallback.
template <typename T>
std::string describe(const T& /*value*/) {
  return "value";
}

/// Chosen for any Entity; the Entity && Named overload subsumes this one.
template <typename T>
  requires Entity<T>
std::string describe(const T& value) {
  return "entity:" + detail::to_display(value.id());
}

/// Chosen for types that are both Entity and Named. The conjunction contains
/// `Entity<T>` as an atomic constraint, so it subsumes the overload above.
template <typename T>
  requires(Entity<T> && Named<T>)
std::string describe(const T& value) {
  return "entity:" + detail::to_display(value.id()) + ":" +
         std::string{value.name()};
}

/// Sum of elements of any input range whose elements convert to long long.
template <std::ranges::input_range R>
  requires std::convertible_to<std::ranges::range_reference_t<R>, long long>
long long total_of(R&& range) {
  long long total = 0;
  for (auto&& element : range) {
    total += static_cast<long long>(element);
  }
  return total;
}

}  // namespace cppref::templates_and_concepts
