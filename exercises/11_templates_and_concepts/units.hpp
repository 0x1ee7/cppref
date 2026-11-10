// Part 5: units of measure. Dimensions are checked at compile time.
#pragma once

#include <compare>
#include <concepts>
#include <stdexcept>

namespace cppref::templates_and_concepts {

/// Exponents of length and time in the SI base dimensions.
template <int Length, int Time>
struct Dim {
  static constexpr int length = Length;
  static constexpr int time = Time;
};

using LengthDim = Dim<1, 0>;
using TimeDim = Dim<0, 1>;
using SpeedDim = Dim<1, -1>;
using AreaDim = Dim<2, 0>;

/// Dimension of a product or quotient of two dimensions.
template <typename A, typename B>
using dim_product = Dim<A::length + B::length, A::time + B::time>;

template <typename A, typename B>
using dim_quotient = Dim<A::length - B::length, A::time - B::time>;

/// A value in SI base units (metres, seconds, ...) tagged with its dimension.
/// Construction is explicit; the underlying value is `double`.
///
/// Addition, subtraction and ordering exist only between quantities of the
/// same dimension. Multiplication and division combine dimensions.
///
/// STUB: functions throw (value() returns 0, noexcept). The engineer keeps the
/// single data member `double value_` (already declared below).
template <typename D>
class Quantity {
 public:
  explicit Quantity(double value) : value_(value) {
    throw std::logic_error("not implemented: Quantity::Quantity");
  }

  /// STUB: returns 0 (noexcept).
  double value() const noexcept { return 0.0; }

  friend Quantity operator+(Quantity lhs, Quantity rhs) {
    (void)lhs;
    (void)rhs;
    throw std::logic_error("not implemented: Quantity::operator+");
  }

  friend Quantity operator-(Quantity lhs, Quantity rhs) {
    (void)lhs;
    (void)rhs;
    throw std::logic_error("not implemented: Quantity::operator-");
  }

  friend std::partial_ordering operator<=>(Quantity lhs, Quantity rhs) {
    (void)lhs;
    (void)rhs;
    throw std::logic_error("not implemented: Quantity::operator<=>");
  }

  friend bool operator==(Quantity lhs, Quantity rhs) {
    (void)lhs;
    (void)rhs;
    throw std::logic_error("not implemented: Quantity::operator==");
  }

 private:
  double value_{};
};

/// Dimensional products and quotients.
template <typename A, typename B>
Quantity<dim_product<A, B>> operator*(Quantity<A> lhs, Quantity<B> rhs) {
  (void)lhs;
  (void)rhs;
  throw std::logic_error("not implemented: Quantity operator*");
}

template <typename A, typename B>
Quantity<dim_quotient<A, B>> operator/(Quantity<A> lhs, Quantity<B> rhs) {
  (void)lhs;
  (void)rhs;
  throw std::logic_error("not implemented: Quantity operator/");
}

/// Scaling by a dimensionless factor.
template <typename D>
Quantity<D> operator*(Quantity<D> lhs, double factor) {
  (void)lhs;
  (void)factor;
  throw std::logic_error("not implemented: Quantity operator* (scalar)");
}

/// True when `Quantity<A> + Quantity<B>` is well-formed.
template <typename A, typename B>
concept CanAdd = requires(Quantity<A> a, Quantity<B> b) { a + b; };

using Meters = Quantity<LengthDim>;
using Seconds = Quantity<TimeDim>;
using MetersPerSecond = Quantity<SpeedDim>;
using SquareMeters = Quantity<AreaDim>;

/// Constructors for readability.
inline Meters meters(double value) {
  (void)value;
  throw std::logic_error("not implemented: meters");
}

inline Seconds seconds(double value) {
  (void)value;
  throw std::logic_error("not implemented: seconds");
}

}  // namespace cppref::templates_and_concepts
