// Part 4: calendar arithmetic, billing dates, business-day calendar.
#include "calendar.hpp"
#include "test_support_test.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

namespace {

using namespace cppref::chrono_time;
using namespace std::chrono;

year_month_day ymd(int y, unsigned m, unsigned d) {
    return year_month_day{year{y}, month{m}, day{d}};
}

TEST(Part4Calendar, DaysInMonthHandlesLeapYears) {
    EXPECT_EQ(days_in_month(year_month{year{2024}, month{2}}), 29u);
    EXPECT_EQ(days_in_month(year_month{year{2025}, month{2}}), 28u);
    EXPECT_EQ(days_in_month(year_month{year{2026}, month{4}}), 30u);
    EXPECT_THROW(days_in_month(year_month{year{2026}, month{13}}), std::invalid_argument);
}

TEST(Part4Calendar, ParseDateAcceptsValidAndRejectsImpossible) {
    EXPECT_EQ(parse_date("2026-03-29"), ymd(2026, 3, 29));
    EXPECT_THROW(parse_date("2026-02-30"), std::invalid_argument);
    EXPECT_THROW(parse_date("29/03/2026"), std::invalid_argument);
}

TEST(Part4Calendar, AddMonthsClampsToEndOfMonth) {
    EXPECT_EQ(add_months(ymd(2026, 1, 31), months{1}), ymd(2026, 2, 28));
    EXPECT_EQ(add_months(ymd(2024, 1, 31), months{1}), ymd(2024, 2, 29));
}

TEST(Part4Calendar, AddMonthsDoesNotStickyClamp) {
    // The clamp applies to a single result; it must not persist from the anchor.
    EXPECT_EQ(add_months(ymd(2026, 2, 28), months{1}), ymd(2026, 3, 28));
}

TEST(Part4Calendar, AddMonthsNegativeCrossesYearBoundary) {
    EXPECT_EQ(add_months(ymd(2026, 1, 15), months{-2}), ymd(2025, 11, 15));
    EXPECT_EQ(add_months(ymd(2026, 1, 15), months{0}), ymd(2026, 1, 15));
}

TEST(Part4Calendar, AddMonthsRejectsInvalidInput) {
    EXPECT_THROW(add_months(year_month_day{}, months{1}), std::invalid_argument);
}

TEST(Part4Calendar, BillingDatesDoNotDrift) {
    const auto dates = billing_dates(ymd(2026, 1, 31), 4);
    ASSERT_EQ(dates.size(), 4u);
    EXPECT_EQ(dates[0], ymd(2026, 1, 31));
    EXPECT_EQ(dates[1], ymd(2026, 2, 28));
    EXPECT_EQ(dates[2], ymd(2026, 3, 31));
    EXPECT_EQ(dates[3], ymd(2026, 4, 30));
}

TEST(Part4Calendar, BillingDatesCountEdgeCases) {
    EXPECT_TRUE(billing_dates(ymd(2026, 1, 31), 0).empty());
    EXPECT_THROW(billing_dates(ymd(2026, 1, 31), -1), std::invalid_argument);
}

TEST(Part4Calendar, LastWeekdayFindsLastMondayOfMay) {
    EXPECT_EQ(last_weekday(year_month{year{2026}, month{5}}, Monday), ymd(2026, 5, 25));
}

TEST(Part4Calendar, IsBusinessDayRespectsWeekendAndHolidays) {
    const BusinessCalendar cal{{ymd(2026, 3, 30)}};
    EXPECT_TRUE(cal.is_business_day(ymd(2026, 3, 27)));   // Friday
    EXPECT_FALSE(cal.is_business_day(ymd(2026, 3, 28)));  // Saturday
    EXPECT_FALSE(cal.is_business_day(ymd(2026, 3, 30)));  // Monday holiday
}

TEST(Part4Calendar, AddBusinessDaysSkipsWeekend) {
    const BusinessCalendar cal;
    EXPECT_EQ(cal.add_business_days(ymd(2026, 3, 27), 1), ymd(2026, 3, 30));
    EXPECT_EQ(cal.add_business_days(ymd(2026, 3, 28), 1), ymd(2026, 3, 30));  // from Saturday
    EXPECT_EQ(cal.add_business_days(ymd(2026, 3, 30), -1), ymd(2026, 3, 27));
    EXPECT_EQ(cal.add_business_days(ymd(2026, 3, 30), 0), ymd(2026, 3, 30));
}

TEST(Part4Calendar, AddBusinessDaysSkipsHolidayChains) {
    const BusinessCalendar cal{{ymd(2026, 3, 30), ymd(2026, 3, 31)}};
    EXPECT_EQ(cal.add_business_days(ymd(2026, 3, 27), 1), ymd(2026, 4, 1));
}

TEST(Part4Calendar, BusinessDaysBetweenIsSigned) {
    const BusinessCalendar cal;
    EXPECT_EQ(cal.business_days_between(ymd(2026, 3, 27), ymd(2026, 3, 30)), 1);
    EXPECT_EQ(cal.business_days_between(ymd(2026, 3, 30), ymd(2026, 3, 27)), -1);
    EXPECT_EQ(cal.business_days_between(ymd(2026, 3, 27), ymd(2026, 3, 27)), 0);
}

TEST(Part4Calendar, ConstructorRejectsNoWorkdaysAndInvalidHolidays) {
    const std::vector<weekday> all{Monday, Tuesday, Wednesday, Thursday, Friday, Saturday, Sunday};
    EXPECT_THROW((BusinessCalendar{{}, all}), std::invalid_argument);
    EXPECT_THROW((BusinessCalendar{{year_month_day{}}}), std::invalid_argument);
}

}  // namespace
