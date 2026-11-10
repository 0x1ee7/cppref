#include "etl.hpp"

#include "composition.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <expected>
#include <format>
#include <map>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cppref::functional {

namespace {

constexpr std::size_t npos = std::string_view::npos;

std::string_view trim(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }
    return text;
}

std::string lowercase(std::string_view text) {
    std::string out(text);
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

std::string uppercase(std::string_view text) {
    std::string out(text);
    for (char& c : out) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return out;
}

bool is_digit(char c) { return c >= '0' && c <= '9'; }

std::string_view find_field(const RawRow& row, std::string_view key, bool& missing) {
    auto it = row.find(std::string(key));
    if (it == row.end()) {
        missing = true;
        return {};
    }
    return it->second;
}

}  // namespace

std::expected<std::int64_t, std::string> parse_cents(std::string_view raw) {
    auto invalid = [raw]() -> std::unexpected<std::string> {
        return std::unexpected(std::format("invalid amount '{}'", raw));
    };

    std::string_view whole = raw;
    std::string_view fraction;
    if (const std::size_t dot = raw.find('.'); dot != npos) {
        whole = raw.substr(0, dot);
        fraction = raw.substr(dot + 1);
        if (fraction.empty() || fraction.size() > 2) {
            return invalid();
        }
    }
    if (whole.empty() || whole.size() > 15) {
        return invalid();
    }
    for (char c : whole) {
        if (!is_digit(c)) {
            return invalid();
        }
    }
    for (char c : fraction) {
        if (!is_digit(c)) {
            return invalid();
        }
    }

    std::int64_t dollars = 0;
    for (char c : whole) {
        dollars = dollars * 10 + (c - '0');
    }
    std::int64_t cents = 0;
    for (char c : fraction) {
        cents = cents * 10 + (c - '0');
    }
    if (fraction.size() == 1) {
        cents *= 10;  // "12.5" means 50 cents
    }
    return dollars * 100 + cents;
}

std::expected<std::chrono::year_month_day, std::string> parse_date(std::string_view raw) {
    auto invalid = [raw]() -> std::unexpected<std::string> {
        return std::unexpected(std::format("invalid date '{}'", raw));
    };
    if (raw.size() != 10 || raw[4] != '-' || raw[7] != '-') {
        return invalid();
    }
    for (std::size_t i : {0, 1, 2, 3, 5, 6, 8, 9}) {
        if (!is_digit(raw[i])) {
            return invalid();
        }
    }
    auto number = [](std::string_view digits) {
        int value = 0;
        for (char c : digits) {
            value = value * 10 + (c - '0');
        }
        return value;
    };
    const std::chrono::year_month_day date{
        std::chrono::year{number(raw.substr(0, 4))},
        std::chrono::month{static_cast<unsigned>(number(raw.substr(5, 2)))},
        std::chrono::day{static_cast<unsigned>(number(raw.substr(8, 2)))},
    };
    if (!date.ok()) {
        return invalid();
    }
    return date;
}

std::expected<Sale, std::string> parse_sale(const RawRow& row) {
    bool missing = false;
    const std::string_view region_raw = find_field(row, "region", missing);
    const std::string_view product_raw = find_field(row, "product", missing);
    const std::string_view units_raw = find_field(row, "units", missing);
    const std::string_view price_raw = find_field(row, "unit_price", missing);
    const std::string_view date_raw = find_field(row, "sold_on", missing);
    if (missing) {
        for (std::string_view key : {"region", "product", "units", "unit_price", "sold_on"}) {
            if (!row.contains(std::string(key))) {
                return std::unexpected(std::format("missing field '{}'", key));
            }
        }
    }

    Sale sale;
    sale.region = lowercase(trim(region_raw));
    if (sale.region.empty()) {
        return std::unexpected("region must not be empty");
    }
    sale.product = std::string(trim(product_raw));

    const std::string_view units_text = trim(units_raw);
    int units = 0;
    const auto [ptr, ec] = std::from_chars(units_text.data(), units_text.data() + units_text.size(), units);
    if (ec != std::errc{} || ptr != units_text.data() + units_text.size()) {
        return std::unexpected(std::format("invalid units '{}'", units_text));
    }
    if (units < 0) {
        return std::unexpected("units must not be negative");
    }
    sale.units = units;

    auto cents = parse_cents(trim(price_raw));
    if (!cents) {
        return std::unexpected(cents.error());
    }
    sale.unit_price_cents = *cents;

    auto date = parse_date(trim(date_raw));
    if (!date) {
        return std::unexpected(date.error());
    }
    sale.sold_on = *date;
    return sale;
}

std::expected<std::vector<Sale>, std::string> clean_sales(std::span<const RawRow> rows) {
    std::vector<Sale> sales;
    sales.reserve(rows.size());
    for (const RawRow& row : rows) {
        auto region = row.find("region");
        if (region != row.end() && trim(region->second).empty()) {
            continue;
        }
        auto units = row.find("units");
        if (units != row.end() && trim(units->second) == "0") {
            continue;
        }
        auto sale = parse_sale(row);
        if (!sale) {
            return std::unexpected(sale.error());
        }
        sales.push_back(std::move(*sale));
    }
    return sales;
}

std::int64_t revenue_cents(const Sale& sale) {
    return static_cast<std::int64_t>(sale.units) * sale.unit_price_cents;
}

std::vector<std::pair<std::string, std::int64_t>> ranked(
    const std::map<std::string, std::int64_t>& totals) {
    std::vector<std::pair<std::string, std::int64_t>> entries(totals.begin(), totals.end());
    std::ranges::sort(entries, [](const auto& a, const auto& b) {
        if (a.second != b.second) {
            return a.second > b.second;
        }
        return a.first < b.first;
    });
    return entries;
}

std::vector<std::string> distinct_regions(std::span<const Sale> sales) {
    std::set<std::string> regions;
    for (const Sale& sale : sales) {
        regions.insert(uppercase(sale.region));
    }
    return {regions.begin(), regions.end()};
}

std::expected<std::string, std::string> format_cents(std::int64_t cents, std::string_view currency) {
    if (cents < 0) {
        return std::unexpected("amount must not be negative");
    }
    const std::int64_t whole = cents / 100;
    const std::int64_t fraction = cents % 100;

    const std::string digits = std::to_string(whole);
    std::string grouped;
    grouped.reserve(digits.size() + digits.size() / 3);
    for (std::size_t i = 0; i < digits.size(); ++i) {
        if (i > 0 && (digits.size() - i) % 3 == 0) {
            grouped.push_back(',');
        }
        grouped.push_back(digits[i]);
    }
    return std::format("{}{}.{:02}", currency, grouped, fraction);
}

std::expected<std::vector<std::string>, std::string> build_report(std::span<const RawRow> rows,
                                                                  std::string_view currency) {
    using Totals = std::vector<std::pair<std::string, std::int64_t>>;
    return pipe(
        rows,
        [](std::span<const RawRow> r) { return clean_sales(r); },
        [](const std::expected<std::vector<Sale>, std::string>& sales) {
            return sales.transform([](const std::vector<Sale>& v) {
                return ranked(totals_by(v, &Sale::region));
            });
        },
        [currency](const std::expected<Totals, std::string>& entries)
            -> std::expected<std::vector<std::string>, std::string> {
            if (!entries) {
                return std::unexpected(entries.error());
            }
            std::vector<std::string> lines;
            lines.reserve(entries->size());
            for (const auto& [region, cents] : *entries) {
                auto amount = format_cents(cents, currency);
                if (!amount) {
                    return std::unexpected(amount.error());
                }
                lines.push_back(region + ": " + *amount);
            }
            return lines;
        });
}

}  // namespace cppref::functional
