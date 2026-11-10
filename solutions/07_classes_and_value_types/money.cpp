#include "money.hpp"

#include <limits>
#include <optional>
#include <ostream>
#include <utility>
#include <vector>

namespace cppref::classes {
namespace {

constexpr std::uint64_t kScale = 1'000'000;  // 10^Money::kInternalDigits
constexpr std::uint64_t kLimitPositive = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
constexpr std::uint64_t kLimitNegative = kLimitPositive + 1;  // |INT64_MIN|

constexpr std::uint64_t pow10(int n) {
    std::uint64_t result = 1;
    for (int i = 0; i < n; ++i) {
        result *= 10;
    }
    return result;
}

std::uint64_t magnitude(std::int64_t value) {
    return value < 0 ? std::uint64_t{0} - static_cast<std::uint64_t>(value) : static_cast<std::uint64_t>(value);
}

/// Signed value from a magnitude, or nullopt if it does not fit int64.
std::optional<std::int64_t> from_magnitude(std::uint64_t mag, bool negative) {
    if (!negative) {
        if (mag > kLimitPositive) {
            return std::nullopt;
        }
        return static_cast<std::int64_t>(mag);
    }
    if (mag > kLimitNegative) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(std::uint64_t{0} - mag);
}

/// acc = acc * factor + addend, unless the result would exceed limit.
bool scale_add(std::uint64_t& acc, std::uint64_t factor, std::uint64_t addend, std::uint64_t limit) {
    if (acc > (limit - addend) / factor) {
        return false;
    }
    acc = acc * factor + addend;
    return true;
}

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

void require_same_currency(const Currency& lhs, const Currency& rhs) {
    if (lhs != rhs) {
        throw CurrencyMismatchError("currency mismatch: " + std::string(lhs.code()) + " vs " +
                                    std::string(rhs.code()));
    }
}

}  // namespace

// ---------------------------------------------------------------- Currency

std::expected<Currency, MoneyError> Currency::parse(std::string_view code) {
    if (code.size() != 3) {
        return std::unexpected(MoneyError::invalid_currency);
    }
    for (char c : code) {
        if (c < 'A' || c > 'Z') {
            return std::unexpected(MoneyError::invalid_currency);
        }
    }
    return Currency(std::array<char, 3>{code[0], code[1], code[2]});
}

std::string_view Currency::code() const {
    return {code_.data(), code_.size()};
}

int Currency::minor_digits() const {
    const auto text = code();
    if (text == "JPY" || text == "KRW") {
        return 0;
    }
    if (text == "KWD") {
        return 3;
    }
    return 2;
}

// ---------------------------------------------------------------- Money: construction

std::expected<Money, MoneyError> Money::from_minor(std::int64_t minor_units, Currency currency) {
    const std::uint64_t factor = pow10(kInternalDigits - currency.minor_digits());
    const bool negative = minor_units < 0;
    const std::uint64_t mag = magnitude(minor_units);
    const std::uint64_t limit = negative ? kLimitNegative : kLimitPositive;
    if (mag > limit / factor) {
        return std::unexpected(MoneyError::overflow);
    }
    return Money(*from_magnitude(mag * factor, negative), currency);
}

std::expected<Money, MoneyError> Money::parse(std::string_view text, Currency currency) {
    std::size_t pos = 0;
    bool negative = false;
    if (pos < text.size() && (text[pos] == '+' || text[pos] == '-')) {
        negative = text[pos] == '-';
        ++pos;
    }
    const std::uint64_t limit = negative ? kLimitNegative : kLimitPositive;

    std::uint64_t whole = 0;
    std::size_t int_digits = 0;
    while (pos < text.size() && is_digit(text[pos])) {
        if (!scale_add(whole, 10, static_cast<std::uint64_t>(text[pos] - '0'), limit)) {
            return std::unexpected(MoneyError::overflow);
        }
        ++int_digits;
        ++pos;
    }
    if (int_digits == 0) {
        return std::unexpected(MoneyError::invalid_amount);
    }
    if (!scale_add(whole, kScale, 0, limit)) {
        return std::unexpected(MoneyError::overflow);
    }

    std::uint64_t frac = 0;
    std::size_t frac_digits = 0;
    if (pos < text.size() && text[pos] == '.') {
        ++pos;
        while (pos < text.size() && is_digit(text[pos])) {
            if (frac_digits == static_cast<std::size_t>(kInternalDigits)) {
                return std::unexpected(MoneyError::invalid_amount);
            }
            frac = frac * 10 + static_cast<std::uint64_t>(text[pos] - '0');
            ++frac_digits;
            ++pos;
        }
        if (frac_digits == 0) {
            return std::unexpected(MoneyError::invalid_amount);
        }
    }
    if (pos != text.size()) {
        return std::unexpected(MoneyError::invalid_amount);
    }

    frac *= pow10(kInternalDigits - static_cast<int>(frac_digits));
    if (frac > limit - whole) {
        return std::unexpected(MoneyError::overflow);
    }
    whole += frac;
    return Money(*from_magnitude(whole, negative), currency);
}

Money Money::zero(Currency currency) {
    return Money(0, currency);
}

// ---------------------------------------------------------------- Money: queries

Currency Money::currency() const {
    return currency_;
}

bool Money::is_zero() const {
    return micro_ == 0;
}

bool Money::is_negative() const {
    return micro_ < 0;
}

bool Money::is_positive() const {
    return micro_ > 0;
}

Money Money::rounded() const {
    const std::uint64_t step = pow10(kInternalDigits - currency_.minor_digits());
    const bool negative = micro_ < 0;
    const std::uint64_t mag = magnitude(micro_);

    std::uint64_t units = mag / step;
    const std::uint64_t twice_rem = (mag % step) * 2;
    // Half-even: round up on more than half, or exactly half and the quotient is odd.
    if (twice_rem > step || (twice_rem == step && units % 2 == 1)) {
        ++units;
    }
    const auto micro = from_magnitude(units * step, negative);
    if (!micro) {
        throw std::overflow_error("rounded: result does not fit");
    }
    return Money(*micro, currency_);
}

std::expected<std::vector<Money>, MoneyError> Money::allocate(int parts) const {
    if (parts < 1) {
        return std::unexpected(MoneyError::invalid_parts);
    }
    const std::uint64_t step = pow10(kInternalDigits - currency_.minor_digits());
    const std::uint64_t mag = magnitude(micro_);
    if (mag % step != 0) {
        return std::unexpected(MoneyError::precision_loss);
    }

    const bool negative = micro_ < 0;
    const std::uint64_t units = mag / step;
    const auto count = static_cast<std::uint64_t>(parts);
    const std::uint64_t base = units / count;
    const std::uint64_t remainder = units % count;

    std::vector<Money> shares;
    shares.reserve(count);
    for (std::uint64_t i = 0; i < count; ++i) {
        const std::uint64_t share_units = base + (i < remainder ? 1 : 0);
        shares.push_back(Money(*from_magnitude(share_units * step, negative), currency_));
    }
    return shares;
}

std::string Money::to_string() const {
    const std::uint64_t mag = magnitude(micro_);
    const std::uint64_t whole = mag / kScale;
    std::uint64_t frac = mag % kScale;

    std::string frac_digits(static_cast<std::size_t>(kInternalDigits), '0');
    for (std::size_t i = frac_digits.size(); i-- > 0;) {
        frac_digits[i] = static_cast<char>('0' + frac % 10);
        frac /= 10;
    }
    const std::size_t needed = frac_digits.find_last_not_of('0') + 1;  // npos + 1 == 0 when all zero
    const std::size_t shown = std::max(static_cast<std::size_t>(currency_.minor_digits()), needed);

    std::string out;
    if (micro_ < 0) {
        out.push_back('-');
    }
    out += std::to_string(whole);
    if (shown > 0) {
        out.push_back('.');
        out.append(frac_digits, 0, shown);
    }
    out.push_back(' ');
    out += currency_.code();
    return out;
}

// ---------------------------------------------------------------- Money: operators

Money operator+(const Money& lhs, const Money& rhs) {
    require_same_currency(lhs.currency_, rhs.currency_);
    constexpr auto max = std::numeric_limits<std::int64_t>::max();
    constexpr auto min = std::numeric_limits<std::int64_t>::min();
    if (rhs.micro_ > 0 ? lhs.micro_ > max - rhs.micro_ : lhs.micro_ < min - rhs.micro_) {
        throw std::overflow_error("money addition overflow");
    }
    return Money(lhs.micro_ + rhs.micro_, lhs.currency_);
}

Money operator-(const Money& lhs, const Money& rhs) {
    require_same_currency(lhs.currency_, rhs.currency_);
    constexpr auto max = std::numeric_limits<std::int64_t>::max();
    constexpr auto min = std::numeric_limits<std::int64_t>::min();
    if (rhs.micro_ > 0 ? lhs.micro_ < min + rhs.micro_ : lhs.micro_ > max + rhs.micro_) {
        throw std::overflow_error("money subtraction overflow");
    }
    return Money(lhs.micro_ - rhs.micro_, lhs.currency_);
}

Money operator-(const Money& value) {
    if (value.micro_ == std::numeric_limits<std::int64_t>::min()) {
        throw std::overflow_error("money negation overflow");
    }
    return Money(-value.micro_, value.currency_);
}

Money operator*(const Money& value, std::int64_t factor) {
    if (value.micro_ == 0 || factor == 0) {
        return Money(0, value.currency_);
    }
    const bool negative = (value.micro_ < 0) != (factor < 0);
    const std::uint64_t limit = negative ? kLimitNegative : kLimitPositive;
    const std::uint64_t a = magnitude(value.micro_);
    const std::uint64_t b = magnitude(factor);
    if (a > limit / b) {
        throw std::overflow_error("money multiplication overflow");
    }
    return Money(*from_magnitude(a * b, negative), value.currency_);
}

Money operator*(std::int64_t factor, const Money& value) {
    return value * factor;
}

bool operator==(const Money& lhs, const Money& rhs) {
    return lhs.currency_ == rhs.currency_ && lhs.micro_ == rhs.micro_;
}

std::strong_ordering operator<=>(const Money& lhs, const Money& rhs) {
    require_same_currency(lhs.currency_, rhs.currency_);
    return lhs.micro_ <=> rhs.micro_;
}

std::ostream& operator<<(std::ostream& out, const Money& value) {
    return out << value.to_string();
}

}  // namespace cppref::classes

std::size_t std::hash<cppref::classes::Money>::operator()(const cppref::classes::Money& value) const {
    const std::size_t amount = std::hash<std::int64_t>{}(value.micro_);
    const std::size_t code = std::hash<std::string_view>{}(value.currency_.code());
    return amount * 31u ^ code;
}
