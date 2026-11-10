#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace cppref::error_handling {

/// Money in integer cents.
struct Money {
    std::int64_t cents = 0;

    auto operator<=>(const Money&) const = default;
};

/// Why a piece of text was rejected.
enum class ParseFailure {
    empty,              // nothing to parse
    bad_character,      // not a valid number in this position
    too_many_decimals,  // amount has more than two fractional digits
    out_of_range,       // does not fit, or a quantity that must be positive is zero
    wrong_field_count,  // line item does not have exactly three fields
};

struct LineItem {
    std::string sku;
    int quantity = 0;
    Money price;

    friend bool operator==(const LineItem&, const LineItem&) = default;
};

/// Which field failed (0 = sku, 1 = quantity, 2 = price) and why. For
/// wrong_field_count, `field` is the number of fields found.
struct LineItemError {
    std::size_t field = 0;
    ParseFailure reason = ParseFailure::empty;

    friend bool operator==(const LineItemError&, const LineItemError&) = default;
};

/// Accepts "12", "12.3", "12.34". No sign, no exponent, no thousands separators.
/// A fraction needs at least one digit when a '.' is present.
[[nodiscard]] std::expected<Money, ParseFailure> parse_amount(std::string_view text);

/// Accepts a positive integer that fits in int.
[[nodiscard]] std::expected<int, ParseFailure> parse_quantity(std::string_view text);

/// Parses "sku,quantity,price". Composes parse_quantity and parse_amount with
/// monadic operations, mapping each field's failure to a LineItemError.
[[nodiscard]] std::expected<LineItem, LineItemError> parse_line_item(std::string_view line);

}  // namespace cppref::error_handling
