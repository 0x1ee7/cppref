// Part 5: units of measure. Dimensions are checked at compile time.
#pragma once

#include <compare>
#include <concepts>

namespace cppref::templates_and_concepts {

/// Exponents of the SI base dimensions length and time.
template <int Length, int Time>
struct Dim {
  static constexpr int length = Length;
  static constexpr int time = Time;
};

using LengthDim = Dim<1, 0>;
using TimeDim = Dim<0, 1>;
using SpeedDim = Dim<1, -1>;
using AreaDim = Dim<2, 0>;

template <typename A, typename B>
using dim_product = Dim<A::length + B::length, A::time + B::time>;

template <typename A, typename B>
using dim_quotient = Dim<A::length - B::length, A::time - B::time>;

/// A double tagged with its dimension. Explicit construction; arithmetic only
/// within one dimension (hidden friends are found only for matching types).
template <typename D>
class Quantity {
 public:
  constexpr explicit Quantity(double value) noexcept : value_(value) {}

  constexpr double value() const noexcept { return value_; }

  friend constexpr Quantity operator+(Quantity lhs, Quantity rhs) noexcept {
    return Quantity{lhs.value_ + rhs.value_};
  }

  friend constexpr Quantity operator-(Quantity lhs, Quantity rhs) noexcept {
    return Quantity{lhs.value_ - rhs.value_};
  }

  friend constexpr std::partial_ordering operator<=>(Quantity lhs,
                                                     Quantity rhs) noexcept {
    return lhs.value_ <=> rhs.value_;
  }

  friend constexpr bool operator==(Quantity lhs, Quantity rhs) noexcept {
    return lhs.value_ == rhs.value_;
  }

 private:
  double value_{};
};

/// Products and quotients combine dimensions.
template <typename A, typename B>
constexpr Quantity<dim_product<A, B>> operator*(Quantity<A> lhs,
                                                Quantity<B> rhs) noexcept {
  return Quantity<dim_product<A, B>>{lhs.value() * rhs.value()};
}

template <typename A, typename B>
constexpr Quantity<dim_quotient<A, B>> operator/(Quantity<A> lhs,
                                                 Quantity<B> rhs) noexcept {
  return Quantity<dim_quotient<A, B>>{lhs.value() / rhs.value()};
}

/// Scaling by a dimensionless factor.
template <typename D>
constexpr Quantity<D> operator*(Quantity<D> lhs, double factor) noexcept {
  return Quantity<D>{lhs.value() * factor};
}

/// True when `Quantity<A> + Quantity<B>` is well-formed.
template <typename A, typename B>
concept CanAdd = requires(Quantity<A> a, Quantity<B> b) { a + b; };

using Meters = Quantity<LengthDim>;
using Seconds = Quantity<TimeDim>;
using MetersPerSecond = Quantity<SpeedDim>;
using SquareMeters = Quantity<AreaDim>;

inline constexpr Meters meters(double value) noexcept { return Meters{value}; }

inline constexpr Seconds seconds(double value) noexcept {
  return Seconds{value};
}

}  // namespace cppref::templates_and_concepts
