// Part 1 & 2: Money value object — exact, currency-aware, immutable.
#pragma once

#include <array>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <functional>
#include <iosfwd>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace cppref::classes {

/// Error codes for fallible Money operations (returned in std::expected).
enum class MoneyError {
    invalid_currency,  ///< code is not exactly three ASCII uppercase letters
    invalid_amount,    ///< not a decimal literal, or more than 6 fraction digits
    precision_loss,    ///< allocate(): amount is finer than the currency's minor units
    invalid_parts,     ///< allocate(): parts < 1
    overflow,          ///< result does not fit the internal int64 representation
};

/// Thrown by operators when two amounts in different currencies are combined.
class CurrencyMismatchError : public std::invalid_argument {
public:
    using std::invalid_argument::invalid_argument;
};

/// ISO-4217-style currency code. A constructed Currency is always valid.
class Currency {
public:
    /// Exactly three ASCII uppercase letters ("USD", "JPY").
    [[nodiscard]] static std::expected<Currency, MoneyError> parse(std::string_view code);

    [[nodiscard]] std::string_view code() const;

    /// 0 for JPY and KRW, 3 for KWD, 2 for everything else.
    [[nodiscard]] int minor_digits() const;

    friend bool operator==(const Currency&, const Currency&) = default;

private:
    explicit Currency(std::array<char, 3> code) : code_(code) {}

    std::array<char, 3> code_;
};

/// Immutable decimal money. Stored exactly as an int64 count of 10^-6 units.
class Money {
public:
    static constexpr int kInternalDigits = 6;

    /// Amount in minor units of the currency: from_minor(1050, USD) is 10.50 USD.
    [[nodiscard]] static std::expected<Money, MoneyError> from_minor(std::int64_t minor_units, Currency currency);

    /// Decimal text: optional sign, digits, optional '.' followed by 1..6 fraction digits.
    [[nodiscard]] static std::expected<Money, MoneyError> parse(std::string_view amount, Currency currency);

    [[nodiscard]] static Money zero(Currency currency);

    [[nodiscard]] Currency currency() const;
    [[nodiscard]] bool is_zero() const;
    [[nodiscard]] bool is_negative() const;
    [[nodiscard]] bool is_positive() const;

    /// Quantise to the currency's minor units using ROUND_HALF_EVEN.
    /// Throws std::overflow_error if the rounded value does not fit.
    [[nodiscard]] Money rounded() const;

    /// Split into `parts` shares that sum exactly to *this (remainder units go first).
    [[nodiscard]] std::expected<std::vector<Money>, MoneyError> allocate(int parts) const;

    /// Canonical text, e.g. "10.50 USD". Fraction digits: at least the minor digits, more if needed.
    [[nodiscard]] std::string to_string() const;

    friend Money operator+(const Money& lhs, const Money& rhs);
    friend Money operator-(const Money& lhs, const Money& rhs);
    friend Money operator-(const Money& value);
    friend Money operator*(const Money& value, std::int64_t factor);
    friend Money operator*(std::int64_t factor, const Money& value);

    /// Floating-point factors are deleted: money is never multiplied by a float.
    template <std::floating_point F>
    friend Money operator*(const Money&, F) = delete;
    template <std::floating_point F>
    friend Money operator*(F, const Money&) = delete;

    friend bool operator==(const Money& lhs, const Money& rhs);
    friend std::strong_ordering operator<=>(const Money& lhs, const Money& rhs);
    friend std::ostream& operator<<(std::ostream& out, const Money& value);

    friend struct std::hash<Money>;

private:
    Money(std::int64_t micro, Currency currency) : micro_(micro), currency_(currency) {}

    std::int64_t micro_;
    Currency currency_;
};

}  // namespace cppref::classes

template <>
struct std::hash<cppref::classes::Money> {
    std::size_t operator()(const cppref::classes::Money& value) const;
};

template <>
struct std::formatter<cppref::classes::Money> {
    constexpr auto parse(std::format_parse_context& ctx) { return inner_.parse(ctx); }

    template <typename FormatContext>
    auto format(const cppref::classes::Money& value, FormatContext& ctx) const -> typename FormatContext::iterator {
        return inner_.format(value.to_string(), ctx);
    }

private:
    std::formatter<std::string_view> inner_;
};
