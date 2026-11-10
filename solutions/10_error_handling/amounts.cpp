#include "amounts.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <expected>
#include <limits>
#include <string_view>
#include <system_error>

namespace cppref::error_handling {

namespace {

bool all_digits(std::string_view s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
}

}  // namespace

std::expected<Money, ParseFailure> parse_amount(std::string_view text) {
    if (text.empty()) {
        return std::unexpected(ParseFailure::empty);
    }

    const auto dot = text.find('.');
    const std::string_view whole = text.substr(0, dot);
    if (!all_digits(whole)) {
        return std::unexpected(ParseFailure::bad_character);
    }

    std::int64_t fraction = 0;
    if (dot != std::string_view::npos) {
        const std::string_view frac = text.substr(dot + 1);
        if (!all_digits(frac)) {
            return std::unexpected(ParseFailure::bad_character);
        }
        if (frac.size() > 2) {
            return std::unexpected(ParseFailure::too_many_decimals);
        }
        fraction = std::int64_t{frac[0] - '0'} * 10;
        if (frac.size() == 2) {
            fraction += frac[1] - '0';
        }
    }

    std::int64_t units = 0;
    const auto [ptr, ec] = std::from_chars(whole.data(), whole.data() + whole.size(), units);
    if (ec == std::errc::result_out_of_range) {
        return std::unexpected(ParseFailure::out_of_range);
    }
    if (ec != std::errc{} || ptr != whole.data() + whole.size()) {
        return std::unexpected(ParseFailure::bad_character);
    }
    constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
    if (units > (kMax - 99) / 100) {
        return std::unexpected(ParseFailure::out_of_range);
    }
    return Money{units * 100 + fraction};
}

std::expected<int, ParseFailure> parse_quantity(std::string_view text) {
    if (text.empty()) {
        return std::unexpected(ParseFailure::empty);
    }
    if (!all_digits(text)) {
        return std::unexpected(ParseFailure::bad_character);
    }
    int value = 0;
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec == std::errc::result_out_of_range || (ec == std::errc{} && value == 0)) {
        return std::unexpected(ParseFailure::out_of_range);
    }
    if (ec != std::errc{} || ptr != text.data() + text.size()) {
        return std::unexpected(ParseFailure::bad_character);
    }
    return value;
}

std::expected<LineItem, LineItemError> parse_line_item(std::string_view line) {
    const auto fields = static_cast<std::size_t>(std::count(line.begin(), line.end(), ',')) + 1;
    if (fields != 3) {
        return std::unexpected(LineItemError{fields, ParseFailure::wrong_field_count});
    }

    const auto first_comma = line.find(',');
    const auto second_comma = line.find(',', first_comma + 1);
    const std::string_view sku = line.substr(0, first_comma);
    const std::string_view quantity_text = line.substr(first_comma + 1, second_comma - first_comma - 1);
    const std::string_view price_text = line.substr(second_comma + 1);

    if (sku.empty()) {
        return std::unexpected(LineItemError{0, ParseFailure::empty});
    }

    return parse_quantity(quantity_text)
        .transform_error([](ParseFailure f) { return LineItemError{1, f}; })
        .and_then([&](int quantity) {
            return parse_amount(price_text)
                .transform_error([](ParseFailure f) { return LineItemError{2, f}; })
                .transform([&](Money price) { return LineItem{std::string(sku), quantity, price}; });
        });
}

}  // namespace cppref::error_handling
