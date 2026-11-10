// Part 1: concepts, requires-clauses and subsumption.
#pragma once

#include <concepts>
#include <cstddef>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cppref::templates_and_concepts {

/// An entity has a totally ordered `id_type` and a const `id()` accessor
/// returning exactly that type.
///
/// STUB: replace the body with a requires-expression that checks
/// `typename T::id_type`, `std::totally_ordered<id_type>` and
/// `{ t.id() } -> std::same_as<id_type>` on `const T&`.
template <typename T>
concept Entity = true;

/// A named type has a const `name()` convertible to `std::string_view`.
///
/// STUB: replace the body with a requires-expression.
template <typename T>
concept Named = true;

/// Describes a value. Three overloads, selected by subsumption:
///   - unconstrained: returns "value"
///   - requires Entity<T>: returns "entity:<id>"
///   - requires Entity<T> && Named<T>: returns "entity:<id>:<name>"
/// The most constrained viable overload wins.
template <typename T>
std::string describe(const T& value) {
  (void)value;
  throw std::logic_error("not implemented: describe");
}

template <typename T>
  requires Entity<T>
std::string describe(const T& value) {
  (void)value;
  throw std::logic_error("not implemented: describe (Entity)");
}

template <typename T>
  requires(Entity<T> && Named<T>)
std::string describe(const T& value) {
  (void)value;
  throw std::logic_error("not implemented: describe (Entity && Named)");
}

/// Sum of the elements of any input range whose elements convert to
/// `long long`. Accepts lvalue and rvalue ranges (including views).
template <std::ranges::input_range R>
  requires std::convertible_to<std::ranges::range_reference_t<R>, long long>
long long total_of(R&& range) {
  (void)range;
  throw std::logic_error("not implemented: total_of");
}

}  // namespace cppref::templates_and_concepts
