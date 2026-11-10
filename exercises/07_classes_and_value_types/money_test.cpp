// Parts 1-2: Currency and Money (construction, formatting, hashing, arithmetic, rounding, allocation).
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <format>
#include <functional>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <vector>

#include "money.hpp"

namespace cc = cppref::classes;

namespace {

cc::Currency currency(std::string_view code) {
    return cc::Currency::parse(code).value();
}

cc::Money money(std::string_view text, std::string_view code = "USD") {
    return cc::Money::parse(text, currency(code)).value();
}

template <typename T, typename E>
void expect_error(const std::expected<T, E>& result, E expected) {
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), expected);
}

}  // namespace

// ---------------------------------------------------------------- Part 1

TEST(Part1Currency, ParseAcceptsThreeUppercaseLetters) {
    auto eur = cc::Currency::parse("EUR");
    ASSERT_TRUE(eur.has_value());
    EXPECT_EQ(eur->code(), "EUR");
}

TEST(Part1Currency, ParseRejectsMalformedCodes) {
    for (std::string_view bad : {"usd", "US", "USDX", "U5D", ""}) {
        expect_error(cc::Currency::parse(bad), cc::MoneyError::invalid_currency);
    }
}

TEST(Part1Currency, MinorDigitsPerCurrency) {
    EXPECT_EQ(currency("JPY").minor_digits(), 0);
    EXPECT_EQ(currency("KRW").minor_digits(), 0);
    EXPECT_EQ(currency("KWD").minor_digits(), 3);
    EXPECT_EQ(currency("USD").minor_digits(), 2);
}

TEST(Part1Money, ParseKeepsPrecisionAndFormats) {
    EXPECT_EQ(std::format("{}", money("1.005")), "1.005 USD");
    EXPECT_EQ(std::format("{}", money("10.50")), "10.50 USD");
    EXPECT_EQ(std::format("{}", money("-0.5")), "-0.50 USD");
    EXPECT_EQ(std::format("{}", money("100", "JPY")), "100 JPY");
}

TEST(Part1Money, ParseRejectsBadText) {
    for (std::string_view bad : {"", "abc", "1e3", "1.0000001", " 1.00", "1..0", "inf"}) {
        expect_error(cc::Money::parse(bad, currency("USD")), cc::MoneyError::invalid_amount);
    }
}

TEST(Part1Money, FromMinorUsesMinorUnits) {
    EXPECT_EQ(cc::Money::from_minor(1050, currency("USD")).value(), money("10.50"));
    EXPECT_EQ(std::format("{}", cc::Money::from_minor(-5, currency("USD")).value()), "-0.05 USD");
    EXPECT_EQ(std::format("{}", cc::Money::from_minor(1050, currency("JPY")).value()), "1050 JPY");
}

TEST(Part1Money, FromMinorOverflowIsReported) {
    expect_error(cc::Money::from_minor(std::numeric_limits<std::int64_t>::max(), currency("USD")),
                 cc::MoneyError::overflow);
}

TEST(Part1Money, FormatHonoursWidthSpec) {
    EXPECT_EQ(std::format("[{:>12}]", money("10.50")), "[   10.50 USD]");
}

TEST(Part1Money, StreamAndHashAgreeWithEquality) {
    std::ostringstream out;
    out << money("3.5");
    EXPECT_EQ(out.str(), "3.50 USD");

    std::unordered_set<cc::Money> seen{money("1.00"), cc::Money::from_minor(100, currency("USD")).value()};
    EXPECT_EQ(seen.size(), 1u);
    EXPECT_EQ(std::hash<cc::Money>{}(money("1.00")), std::hash<cc::Money>{}(money("1.000")));
}

TEST(Part1Money, EqualityAcrossCurrenciesIsFalseNotAThrow) {
    EXPECT_FALSE(money("1.00") == money("1.00", "EUR"));
    EXPECT_TRUE(money("1.00") != money("1.00", "EUR"));
}

// ---------------------------------------------------------------- Part 2

TEST(Part2Arithmetic, AddSubtractNegate) {
    EXPECT_EQ(money("10.50") + money("2.25"), money("12.75"));
    EXPECT_EQ(money("10.50") - money("12.75"), money("-2.25"));
    EXPECT_EQ(-money("2.25"), money("-2.25"));
}

TEST(Part2Arithmetic, MultiplyByIntegerFromEitherSide) {
    EXPECT_EQ(money("1.10") * 3, money("3.30"));
    EXPECT_EQ(3 * money("1.10"), money("3.30"));
    EXPECT_EQ(money("1.10") * -2, money("-2.20"));
}

TEST(Part2Arithmetic, MismatchedCurrencyThrows) {
    EXPECT_THROW(money("1") + money("1", "EUR"), cc::CurrencyMismatchError);
    EXPECT_THROW(money("1") - money("1", "EUR"), cc::CurrencyMismatchError);
    EXPECT_THROW((void)(money("1") < money("2", "EUR")), cc::CurrencyMismatchError);
}

TEST(Part2Arithmetic, OverflowThrows) {
    const auto max = money("9223372036854.775807");
    EXPECT_THROW(max + money("0.000001"), std::overflow_error);
    EXPECT_THROW(max * 2, std::overflow_error);

    const auto min = money("-9223372036854.775808");
    EXPECT_THROW(-min, std::overflow_error);
}

TEST(Part2Arithmetic, OrderingWithinOneCurrency) {
    EXPECT_LT(money("1.00"), money("2.00"));
    EXPECT_LE(money("2.00"), money("2.00"));
    EXPECT_GT(money("0.01"), money("0.00"));

    std::vector<cc::Money> values{money("3"), money("1"), money("2")};
    std::ranges::sort(values);
    EXPECT_EQ(values.front(), money("1"));
    EXPECT_EQ(values.back(), money("3"));
}

TEST(Part2Arithmetic, AccumulateFromZeroAndSignPredicates) {
    const std::vector<cc::Money> items{money("1.25"), money("2.00"), money("2.75")};
    const auto total = std::accumulate(items.begin(), items.end(), cc::Money::zero(currency("USD")));
    EXPECT_EQ(total, money("6.00"));

    EXPECT_TRUE(cc::Money::zero(currency("USD")).is_zero());
    EXPECT_TRUE(money("-0.01").is_negative());
    EXPECT_TRUE(money("0.01").is_positive());
    EXPECT_FALSE(money("0").is_positive());
}

TEST(Part2Arithmetic, RoundedIsHalfEvenToMinorUnits) {
    EXPECT_EQ(money("1.005").rounded(), money("1.00"));
    EXPECT_EQ(money("1.015").rounded(), money("1.02"));
    EXPECT_EQ(money("1.025").rounded(), money("1.02"));
    EXPECT_EQ(money("2.675").rounded(), money("2.68"));
    EXPECT_EQ(money("-1.005").rounded(), money("-1.00"));
    EXPECT_EQ(money("100.5", "JPY").rounded(), money("100", "JPY"));
    EXPECT_EQ(std::format("{}", money("1.005").rounded()), "1.00 USD");
}

TEST(Part2Arithmetic, AllocateGivesRemainderToFirstShares) {
    auto shares = money("10.00").allocate(3).value();
    ASSERT_EQ(shares.size(), 3u);
    EXPECT_EQ(shares[0], money("3.34"));
    EXPECT_EQ(shares[1], money("3.33"));
    EXPECT_EQ(shares[2], money("3.33"));
    EXPECT_EQ(std::accumulate(shares.begin(), shares.end(), cc::Money::zero(currency("USD"))), money("10.00"));
}

TEST(Part2Arithmetic, AllocateNegativeAmountKeepsSign) {
    auto shares = money("-10.00").allocate(3).value();
    ASSERT_EQ(shares.size(), 3u);
    EXPECT_EQ(shares[0], money("-3.34"));
    EXPECT_EQ(shares[1], money("-3.33"));
    EXPECT_EQ(shares[2], money("-3.33"));
}

TEST(Part2Arithmetic, AllocateWholeUnitCurrency) {
    auto shares = money("10", "JPY").allocate(3).value();
    ASSERT_EQ(shares.size(), 3u);
    EXPECT_EQ(shares[0], money("4", "JPY"));
    EXPECT_EQ(shares[1], money("3", "JPY"));
    EXPECT_EQ(shares[2], money("3", "JPY"));
}

TEST(Part2Arithmetic, AllocateErrors) {
    expect_error(money("10.00").allocate(0), cc::MoneyError::invalid_parts);
    expect_error(money("10.00").allocate(-1), cc::MoneyError::invalid_parts);
    expect_error(money("1.005").allocate(2), cc::MoneyError::precision_loss);
}
