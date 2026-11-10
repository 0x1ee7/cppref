#include "etl.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fn = cppref::functional;
using std::chrono::day;
using std::chrono::month;
using std::chrono::year;
using std::chrono::year_month_day;

namespace {

year_month_day ymd(int y, unsigned m, unsigned d) {
    return year_month_day{year{y}, month{m}, day{d}};
}

fn::RawRow row(std::string region, std::string product, std::string units, std::string price,
               std::string date) {
    return fn::RawRow{{"region", std::move(region)},
                      {"product", std::move(product)},
                      {"units", std::move(units)},
                      {"unit_price", std::move(price)},
                      {"sold_on", std::move(date)}};
}

std::vector<fn::Sale> sample_sales() {
    return {
        {"us", "A", 2, 1000, ymd(2024, 1, 1)},
        {"eu", "B", 1, 3000, ymd(2024, 1, 2)},
        {"us", "B", 1, 550, ymd(2024, 1, 3)},
    };
}

}  // namespace

TEST(Part7EtlReport, ParseCentsIsExactDecimal) {
    EXPECT_EQ(fn::parse_cents("12.5").value(), 1250);
    EXPECT_EQ(fn::parse_cents("12").value(), 1200);
    EXPECT_EQ(fn::parse_cents("0.01").value(), 1);
    EXPECT_EQ(fn::parse_cents("0").value(), 0);
    EXPECT_EQ(fn::parse_cents("1234.56").value(), 123456);
}

TEST(Part7EtlReport, ParseCentsRejectsMalformedInput) {
    for (std::string_view bad : {"1.234", "-1.00", "abc", "", "1e3", "inf", "nan", "1.2.3", "."}) {
        EXPECT_FALSE(fn::parse_cents(bad).has_value()) << bad;
    }
}

TEST(Part7EtlReport, ParseDateValidatesTheCalendar) {
    EXPECT_EQ(fn::parse_date("2024-02-29").value(), ymd(2024, 2, 29));
    EXPECT_FALSE(fn::parse_date("2023-02-29").has_value());
    EXPECT_FALSE(fn::parse_date("2024-13-01").has_value());
    EXPECT_FALSE(fn::parse_date("2024/01/01").has_value());
}

TEST(Part7EtlReport, ParseSaleNormalisesFields) {
    auto sale = fn::parse_sale(row(" US ", " Widget ", " 3 ", " 12.50 ", "2024-05-01"));
    ASSERT_TRUE(sale.has_value());
    EXPECT_EQ(*sale, (fn::Sale{"us", "Widget", 3, 1250, ymd(2024, 5, 1)}));
}

TEST(Part7EtlReport, ParseSaleRejectsBadOrMissingFields) {
    EXPECT_FALSE(fn::parse_sale(row("us", "A", "-1", "1.00", "2024-01-01")).has_value());
    EXPECT_FALSE(fn::parse_sale(row("us", "A", "x", "1.00", "2024-01-01")).has_value());
    EXPECT_FALSE(fn::parse_sale(row("us", "A", "1", "1.00", "2023-02-29")).has_value());

    auto missing = row("us", "A", "1", "1.00", "2024-01-01");
    missing.erase("sold_on");
    EXPECT_FALSE(fn::parse_sale(missing).has_value());
}

TEST(Part7EtlReport, CleanSalesDropsBlankAndZeroRowsAndPropagatesErrors) {
    std::vector<fn::RawRow> rows{
        row("us", "A", "2", "1.00", "2024-01-01"),
        row("   ", "B", "1", "1.00", "2024-01-01"),  // blank region
        row("eu", "C", "0", "5.00", "2024-01-01"),   // zero units
        row("eu", "D", "4", "2.50", "2024-01-02"),
    };
    auto cleaned = fn::clean_sales(rows);
    ASSERT_TRUE(cleaned.has_value());
    ASSERT_EQ(cleaned->size(), 2u);
    EXPECT_EQ((*cleaned)[0].product, "A");
    EXPECT_EQ((*cleaned)[1].product, "D");

    std::vector<fn::RawRow> broken{
        row("us", "A", "1", "1.00", "2024-01-01"),
        row("us", "B", "1", "1.00", "2023-02-29"),
    };
    EXPECT_FALSE(fn::clean_sales(broken).has_value());
}

TEST(Part7EtlReport, TotalsByProjectionAndRevenue) {
    auto sales = sample_sales();
    EXPECT_EQ(fn::revenue_cents(sales[0]), 2000);
    EXPECT_EQ((fn::totals_by(sales, &fn::Sale::region)),
              (std::map<std::string, std::int64_t>{{"eu", 3000}, {"us", 2550}}));
    EXPECT_EQ((fn::totals_by(sales, [](const fn::Sale& s) { return s.product; })),
              (std::map<std::string, std::int64_t>{{"A", 2000}, {"B", 3550}}));
}

TEST(Part7EtlReport, RankedSortsByTotalThenName) {
    std::map<std::string, std::int64_t> totals{{"b", 100}, {"a", 100}, {"c", 50}, {"d", 300}};
    EXPECT_EQ((fn::ranked(totals)),
              (std::vector<std::pair<std::string, std::int64_t>>{
                  {"d", 300}, {"a", 100}, {"b", 100}, {"c", 50}}));
}

TEST(Part7EtlReport, DistinctRegionsAreUppercasedSortedUnique) {
    std::vector<fn::Sale> sales{
        {"us", "A", 1, 1, ymd(2024, 1, 1)},
        {"eu", "A", 1, 1, ymd(2024, 1, 1)},
        {"us", "B", 1, 1, ymd(2024, 1, 1)},
        {"apac", "C", 1, 1, ymd(2024, 1, 1)},
    };
    EXPECT_EQ(fn::distinct_regions(sales), (std::vector<std::string>{"APAC", "EU", "US"}));
}

TEST(Part7EtlReport, FormatCentsGroupsThousandsAndRejectsNegative) {
    EXPECT_EQ(fn::format_cents(1234567).value(), "$12,345.67");
    EXPECT_EQ(fn::format_cents(5).value(), "$0.05");
    EXPECT_EQ(fn::format_cents(0).value(), "$0.00");
    EXPECT_EQ(fn::format_cents(100000000).value(), "$1,000,000.00");
    EXPECT_EQ(fn::format_cents(150, "EUR ").value(), "EUR 1.50");
    EXPECT_FALSE(fn::format_cents(-1).has_value());
}

TEST(Part7EtlReport, BuildReportComposesStagesAndRanks) {
    std::vector<fn::RawRow> rows{
        row("US", "A", "2", "10.00", "2024-01-05"),    // us: 2000
        row("eu", "B", "1", "30.00", "2024-01-06"),    // eu: 3000
        row("US", "C", "1", "5.50", "2024-01-07"),     // us: +550 = 2550
        row("apac", "D", "1", "25.50", "2024-01-08"),  // apac: 2550 (ties with us)
        row("apac", "E", "0", "99.00", "2024-01-08"),  // dropped: zero units
        row("  ", "F", "1", "1.00", "2024-01-08"),     // dropped: blank region
    };
    EXPECT_EQ(fn::build_report(rows).value(),
              (std::vector<std::string>{"eu: $30.00", "apac: $25.50", "us: $25.50"}));
}

TEST(Part7EtlReport, BuildReportPropagatesParseErrors) {
    std::vector<fn::RawRow> rows{row("us", "A", "x", "1.00", "2024-01-01")};
    EXPECT_FALSE(fn::build_report(rows).has_value());
}
