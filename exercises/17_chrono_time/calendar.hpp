// Part 4: calendar types, month arithmetic, billing dates, business days.
#pragma once

#include <chrono>
#include <cstddef>
#include <set>
#include <string_view>
#include <vector>

namespace cppref::chrono_time {

// Days in the month. Throws std::invalid_argument if `ym` is not ok() (e.g. month 13).
unsigned days_in_month(std::chrono::year_month ym);

// Strict "YYYY-MM-DD". Throws std::invalid_argument on malformed or invalid dates.
std::chrono::year_month_day parse_date(std::string_view iso);

// Adds calendar months and clamps the day to the target month's length
// (Jan 31 + 1 month == Feb 28/29). Throws std::invalid_argument if `d` is not ok().
std::chrono::year_month_day add_months(std::chrono::year_month_day d, std::chrono::months m);

// `count` dates computed from `anchor` (no drift): anchor, anchor+1 month, anchor+2 months...
// count == 0 gives an empty vector. Negative count throws std::invalid_argument.
std::vector<std::chrono::year_month_day> billing_dates(std::chrono::year_month_day anchor, int count);

// The last given weekday of the month, e.g. the last Monday of May (Memorial Day).
std::chrono::year_month_day last_weekday(std::chrono::year_month ym, std::chrono::weekday wd);

// Business days: not in the weekend set and not a holiday.
class BusinessCalendar {
public:
    // Throws std::invalid_argument if a holiday is not ok() or if the weekend set
    // covers all seven weekdays (no business day would exist).
    explicit BusinessCalendar(
        std::vector<std::chrono::year_month_day> holidays = {},
        std::vector<std::chrono::weekday> weekend = {std::chrono::Saturday, std::chrono::Sunday});

    // Throws std::invalid_argument if `d` is not ok().
    bool is_business_day(std::chrono::year_month_day d) const;

    // Steps |n| business days forward (n > 0) or backward (n < 0). Every step
    // lands on a business day, so from a weekend the first forward step goes to the next
    // business day. n == 0 returns `d` unchanged.
    std::chrono::year_month_day add_business_days(std::chrono::year_month_day d, int n) const;

    // Business days in (start, end]. If end < start, returns the negated count of (end, start].
    int business_days_between(std::chrono::year_month_day start, std::chrono::year_month_day end) const;

private:
    // Engineer: store the holiday set and the weekend set here.
    std::set<std::chrono::year_month_day> holidays_;
    std::vector<std::chrono::weekday> weekend_;
};

}  // namespace cppref::chrono_time
