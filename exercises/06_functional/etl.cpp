#include "etl.hpp"

#include <stdexcept>

// Part 7 · ETL pipeline. Stub: implement every function.

namespace cppref::functional {

std::expected<std::int64_t, std::string> parse_cents(std::string_view) {
    throw std::logic_error("not implemented: parse_cents");
}

std::expected<std::chrono::year_month_day, std::string> parse_date(std::string_view) {
    throw std::logic_error("not implemented: parse_date");
}

std::expected<Sale, std::string> parse_sale(const RawRow&) {
    throw std::logic_error("not implemented: parse_sale");
}

std::expected<std::vector<Sale>, std::string> clean_sales(std::span<const RawRow>) {
    throw std::logic_error("not implemented: clean_sales");
}

std::int64_t revenue_cents(const Sale&) {
    throw std::logic_error("not implemented: revenue_cents");
}

std::vector<std::pair<std::string, std::int64_t>> ranked(
    const std::map<std::string, std::int64_t>&) {
    throw std::logic_error("not implemented: ranked");
}

std::vector<std::string> distinct_regions(std::span<const Sale>) {
    throw std::logic_error("not implemented: distinct_regions");
}

std::expected<std::string, std::string> format_cents(std::int64_t, std::string_view) {
    throw std::logic_error("not implemented: format_cents");
}

std::expected<std::vector<std::string>, std::string> build_report(std::span<const RawRow>,
                                                                  std::string_view) {
    throw std::logic_error("not implemented: build_report");
}

}  // namespace cppref::functional
