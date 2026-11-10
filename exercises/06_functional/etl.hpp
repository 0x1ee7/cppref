#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Part 7 · ETL pipeline and ranked report (engineer-owned).
//
// Raw rows are string maps with keys region, product, units, unit_price (dollars,
// e.g. "12.50") and sold_on (ISO date). Every step is a pure function returning
// std::expected, and build_report composes them with pipe() from composition.hpp.
namespace cppref::functional {

using RawRow = std::map<std::string, std::string>;

struct Sale {
    std::string region;  // trimmed and lowercased
    std::string product;  // trimmed
    int units = 0;
    std::int64_t unit_price_cents = 0;
    std::chrono::year_month_day sold_on;

    friend bool operator==(const Sale&, const Sale&) = default;
};

/// Exact money parsing ("12.5" -> 1250, "12" -> 1200, "0.01" -> 1, "0" -> 0).
/// No floating point. More than two decimals, a sign, exponent, non-numeric or
/// empty input -> error message.
[[nodiscard]] std::expected<std::int64_t, std::string> parse_cents(std::string_view raw);

/// Strict ISO "YYYY-MM-DD" with calendar validation (2023-02-29 is invalid).
[[nodiscard]] std::expected<std::chrono::year_month_day, std::string> parse_date(
    std::string_view raw);

/// Requires every field. Trims values, lowercases region, parses units (int,
/// negative -> error) and unit_price via parse_cents, and sold_on via parse_date.
[[nodiscard]] std::expected<Sale, std::string> parse_sale(const RawRow& row);

/// Drops rows whose region is blank after trimming and rows whose units is "0",
/// then parses the rest in order. The first parse error is returned.
[[nodiscard]] std::expected<std::vector<Sale>, std::string> clean_sales(
    std::span<const RawRow> rows);

[[nodiscard]] std::int64_t revenue_cents(const Sale& sale);

/// Sums revenue per key(sale). `key` is any callable (projection) returning a
/// string-convertible value; result keys are in ascending order.
template <typename KeyFn>
std::map<std::string, std::int64_t> totals_by(std::span<const Sale> sales, KeyFn key) {
    (void)sales;
    (void)key;
    throw std::logic_error("not implemented: totals_by");
}

/// Entries sorted by total descending, ties broken by name ascending. Use
/// ranges algorithms with projections (stable sorts or one tuple projection).
[[nodiscard]] std::vector<std::pair<std::string, std::int64_t>> ranked(
    const std::map<std::string, std::int64_t>& totals);

/// Sorted unique regions, uppercased.
[[nodiscard]] std::vector<std::string> distinct_regions(std::span<const Sale> sales);

/// 1234567 -> "$12,345.67" (currency prefix as given). Negative -> error.
[[nodiscard]] std::expected<std::string, std::string> format_cents(
    std::int64_t cents, std::string_view currency = "$");

/// clean_sales -> totals_by region -> ranked -> "region: amount" lines, built
/// with pipe() from composition.hpp. Errors from clean_sales are propagated.
[[nodiscard]] std::expected<std::vector<std::string>, std::string> build_report(
    std::span<const RawRow> rows, std::string_view currency = "$");

}  // namespace cppref::functional
