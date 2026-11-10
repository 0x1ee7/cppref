// Part 1 & 2: Money value object — exact, currency-aware, immutable.
#pragma once

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
/// Operators cannot return std::expected without making `a + b` unusable, so
/// programmer errors throw. Higher layers check currencies first and report
/// through their own std::expected results.
class CurrencyMismatchError : public std::invalid_argument {
public:
    using std::invalid_argument::invalid_argument;
};

/// ISO-4217-style currency code. A constructed Currency is always valid.
class Currency {
public:
    /// Exactly three ASCII uppercase letters ("USD", "JPY").
    /// Rejects "usd", "US", "USDX", "U5D".
    [[nodiscard]] static std::expected<Currency, MoneyError> parse(std::string_view code);

    [[nodiscard]] std::string_view code() const;

    /// 0 for JPY and KRW, 3 for KWD, 2 for everything else.
    [[nodiscard]] int minor_digits() const;

    friend bool operator==(const Currency&, const Currency&) = default;

    // engineer: private storage, e.g. std::array<char, 3> code_;
};

/// Immutable decimal money. Stored exactly; never a binary floating point.
class Money {
public:
    /// Internal precision: amounts are an int64 count of 10^-kInternalDigits units.
    /// Sub-minor precision is kept, so rounded() and allocate() have real work to do.
    static constexpr int kInternalDigits = 6;

    /// Amount in minor units of the currency: from_minor(1050, USD) is 10.50 USD.
    [[nodiscard]] static std::expected<Money, MoneyError> from_minor(std::int64_t minor_units, Currency currency);

    /// Decimal text: optional sign, digits, optional '.' followed by 1..6 fraction digits.
    /// No exponent, no surrounding spaces. No quantising: "1.005" stays 1.005.
    [[nodiscard]] static std::expected<Money, MoneyError> parse(std::string_view amount, Currency currency);

    [[nodiscard]] static Money zero(Currency currency);

    [[nodiscard]] Currency currency() const;
    [[nodiscard]] bool is_zero() const;
    [[nodiscard]] bool is_negative() const;
    [[nodiscard]] bool is_positive() const;

    /// Quantise to the currency's minor units using ROUND_HALF_EVEN (banker's rounding).
    [[nodiscard]] Money rounded() const;

    /// Split into `parts` shares that sum exactly to *this.
    /// The first shares receive the remainder minor units. Negative amounts are
    /// split by magnitude and the sign is applied to every share.
    /// Errors: invalid_parts (parts < 1), precision_loss (finer than minor units).
    [[nodiscard]] std::expected<std::vector<Money>, MoneyError> allocate(int parts) const;

    // Same-currency arithmetic. Mismatch throws CurrencyMismatchError; int64 overflow throws std::overflow_error.
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

    /// Equal when currency and amount match. Mismatched currency is simply false; never throws.
    friend bool operator==(const Money& lhs, const Money& rhs);

    /// Ordering within one currency. Mismatch throws CurrencyMismatchError.
    friend std::strong_ordering operator<=>(const Money& lhs, const Money& rhs);

    /// Writes the same text as std::format("{}", value): "10.50 USD".
    friend std::ostream& operator<<(std::ostream& out, const Money& value);

    // engineer: private storage, e.g. std::int64_t micro_; Currency currency_;
};

}  // namespace cppref::classes

template <>
struct std::hash<cppref::classes::Money> {
    /// Must agree with operator==: equal Money values hash equally.
    std::size_t operator()(const cppref::classes::Money& value) const;
};

template <>
struct std::formatter<cppref::classes::Money> {
    /// Accepts the standard width/alignment spec, e.g. "{:>12}". Spec parsing is delegated
    /// to std::formatter<std::string_view>; the stub only skips to the closing brace.
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        while (it != ctx.end() && *it != '}') {
            ++it;
        }
        return it;
    }

    template <typename FormatContext>
    auto format(const cppref::classes::Money&, FormatContext&) const -> typename FormatContext::iterator {
        throw std::logic_error("not implemented: std::formatter<Money>::format");
    }
    // engineer: member std::formatter<std::string_view> inner_; and format via to_string
};
