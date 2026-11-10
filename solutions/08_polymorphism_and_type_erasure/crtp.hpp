// Part 3 - CRTP mixins: Comparable, Printable, InstanceCounter.
// Part 4 - CRTP model validation with typed Field<T> members.
#pragma once

#include <compare>
#include <cstddef>
#include <expected>
#include <functional>
#include <ostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace cppref::polymorphism {

/// Mixin that derives ==, !=, <, <=, >, >= and <=> from Derived.
///
/// Requirement: Derived has a const member `sort_key()` returning a tuple-like
/// value with `==` and `<=>`. Equality and ordering are defined only through it.
template <class Derived>
class Comparable {
public:
    // Hidden friends: found by ADL on Derived, and not visible as members of the mixin.
    friend bool operator==(const Derived& lhs, const Derived& rhs) {
        return lhs.sort_key() == rhs.sort_key();
    }

    friend std::strong_ordering operator<=>(const Derived& lhs, const Derived& rhs) {
        return lhs.sort_key() <=> rhs.sort_key();
    }
};

/// Mixin for text output.
///
/// Requirement: Derived has a const member `std::string describe()`.
/// Provides to_string() and a hidden friend operator<< that writes describe().
template <class Derived>
class Printable {
public:
    [[nodiscard]] std::string to_string() const { return static_cast<const Derived&>(*this).describe(); }

    friend std::ostream& operator<<(std::ostream& os, const Derived& value) { return os << value.describe(); }
};

/// Counts live objects of Derived. Constructors (default, copy, move) increment,
/// the destructor decrements. Each Derived type has its own counter, because the
/// counter is a function-local static of a per-Derived instantiation.
template <class Derived>
class InstanceCounter {
public:
    InstanceCounter() { ++count(); }
    InstanceCounter(const InstanceCounter&) { ++count(); }
    InstanceCounter(InstanceCounter&&) { ++count(); }
    // Assignment does not change the number of live objects.
    InstanceCounter& operator=(const InstanceCounter&) = default;
    InstanceCounter& operator=(InstanceCounter&&) = default;
    ~InstanceCounter() { --count(); }

    /// Number of currently alive Derived objects.
    [[nodiscard]] static std::size_t live() { return count(); }

private:
    static std::size_t& count() {
        static std::size_t n = 0;
        return n;
    }
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
    Field(std::string name, T initial = T{}, std::vector<Rule> rules = {})
        : name_(std::move(name)), value_(std::move(initial)), rules_(std::move(rules)) {}

    [[nodiscard]] const std::string& name() const { return name_; }

    [[nodiscard]] const T& get() const { return value_; }

    /// Runs all rules against `value`. On success stores it. On failure returns the
    /// first error and leaves the stored value unchanged.
    [[nodiscard]] std::expected<void, std::string> set(T value) {
        if (auto r = check(value); !r) {
            return r;
        }
        value_ = std::move(value);
        return {};
    }

    /// Runs all rules against the stored value and returns the first error.
    [[nodiscard]] std::expected<void, std::string> validate() const { return check(value_); }

private:
    [[nodiscard]] std::expected<void, std::string> check(const T& candidate) const {
        for (const auto& rule : rules_) {
            if (auto r = rule(candidate); !r) {
                return r;
            }
        }
        return {};
    }

    std::string name_;
    T value_;
    std::vector<Rule> rules_;
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
    /// first failure, or success when all fields are valid. The fold stops at the
    /// first failing field, because && short-circuits.
    [[nodiscard]] std::expected<void, std::string> validate() const {
        const auto& self = static_cast<const Derived&>(*this);
        std::expected<void, std::string> result{};
        std::apply(
            [&self, &result](auto... ptrs) {
                static_cast<void>(((result = check_field(self, ptrs), result.has_value()) && ...));
            },
            Derived::schema);
        return result;
    }

    /// Number of fields listed in Derived::schema.
    [[nodiscard]] static std::size_t field_count() {
        return std::tuple_size_v<std::remove_cvref_t<decltype(Derived::schema)>>;
    }

private:
    template <class F>
    static std::expected<void, std::string> check_field(const Derived& self, F Derived::*ptr) {
        const auto& field = self.*ptr;
        auto r = field.validate();
        if (!r) {
            return std::unexpected(field.name() + ": " + r.error());
        }
        return {};
    }
};

}  // namespace cppref::polymorphism
