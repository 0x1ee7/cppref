// Part 3 - CRTP mixins: Comparable, Printable, InstanceCounter.
// Part 4 - CRTP model validation with typed Field<T> members.
#pragma once

#include <compare>
#include <cstddef>
#include <expected>
#include <functional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace cppref::polymorphism {

/// Mixin that derives ==, !=, <, <=, >, >= and <=> from Derived.
///
/// Requirement: Derived has a const member `sort_key()` returning a tuple-like
/// value with `==` and `<=>`. Equality and ordering are defined only through it.
template <class Derived>
class Comparable {
public:
    friend bool operator==(const Derived& lhs, const Derived& rhs) {
        (void)lhs;
        (void)rhs;
        throw std::logic_error("not implemented: Comparable::operator==");
    }

    friend std::strong_ordering operator<=>(const Derived& lhs, const Derived& rhs) {
        (void)lhs;
        (void)rhs;
        throw std::logic_error("not implemented: Comparable::operator<=>");
    }
};

/// Mixin for text output.
///
/// Requirement: Derived has a const member `std::string describe()`.
/// Provides to_string() and a hidden friend operator<< that writes describe().
template <class Derived>
class Printable {
public:
    [[nodiscard]] std::string to_string() const { throw std::logic_error("not implemented: Printable::to_string"); }

    friend std::ostream& operator<<(std::ostream& os, const Derived& value) {
        (void)os;
        (void)value;
        throw std::logic_error("not implemented: Printable::operator<<");
    }
};

/// Counts live objects of Derived. Constructors (default, copy, move) increment,
/// the destructor decrements. Each Derived type has its own counter.
///
/// Engineer: add the static counter (remember to define it inline).
template <class Derived>
class InstanceCounter {
public:
    InstanceCounter() { throw std::logic_error("not implemented: InstanceCounter::InstanceCounter"); }
    InstanceCounter(const InstanceCounter&) {
        throw std::logic_error("not implemented: InstanceCounter copy constructor");
    }
    InstanceCounter(InstanceCounter&&) { throw std::logic_error("not implemented: InstanceCounter move constructor"); }
    InstanceCounter& operator=(const InstanceCounter&) = default;
    InstanceCounter& operator=(InstanceCounter&&) = default;
    ~InstanceCounter() = default;

    /// Number of currently alive Derived objects.
    [[nodiscard]] static std::size_t live() { throw std::logic_error("not implemented: InstanceCounter::live"); }
};

/// A typed, validated field.
///
/// A rule returns an error reason (no field-name prefix) for an invalid value.
/// Rules run in the order given; the first failure is reported.
template <class T>
class Field {
public:
    using value_type = T;
    using Rule = std::function<std::expected<void, std::string>(const T&)>;

    /// `initial` is stored as given and is not validated. validate() reports problems.
    Field(std::string /*name*/, T /*initial*/ = T{}, std::vector<Rule> /*rules*/ = {}) {
        throw std::logic_error("not implemented: Field::Field");
    }

    [[nodiscard]] const std::string& name() const { throw std::logic_error("not implemented: Field::name"); }

    [[nodiscard]] const T& get() const { throw std::logic_error("not implemented: Field::get"); }

    /// Runs all rules against `value`. On success stores it. On failure returns the
    /// first error and leaves the stored value unchanged.
    [[nodiscard]] std::expected<void, std::string> set(T /*value*/) {
        throw std::logic_error("not implemented: Field::set");
    }

    /// Runs all rules against the stored value and returns the first error.
    [[nodiscard]] std::expected<void, std::string> validate() const {
        throw std::logic_error("not implemented: Field::validate");
    }

    // Engineer: data members (name, value, rules).
};

/// CRTP base that validates a model's fields.
///
/// Requirement: Derived declares
///     static constexpr auto schema = std::tuple{&Derived::a, &Derived::b, ...};
/// with pointers to Field<T> data members, in validation order.
template <class Derived>
class Model {
public:
    /// Validates the schema fields in order. Returns "<field>: <reason>" for the
    /// first failure, or success when all fields are valid.
    [[nodiscard]] std::expected<void, std::string> validate() const {
        throw std::logic_error("not implemented: Model::validate");
    }

    /// Number of fields listed in Derived::schema.
    [[nodiscard]] static std::size_t field_count() { throw std::logic_error("not implemented: Model::field_count"); }
};

}  // namespace cppref::polymorphism
