#include "calendar.hpp"

#include <algorithm>
#include <charconv>
#include <ranges>
#include <stdexcept>
#include <utility>

namespace cppref::chrono_time {

using namespace std::chrono;

namespace {

bool all_digits(std::string_view s) {
    return !s.empty() && std::ranges::all_of(s, [](char c) { return c >= '0' && c <= '9'; });
}

unsigned parse_unsigned(std::string_view s) {
    unsigned v = 0;
    const auto res = std::from_chars(s.data(), s.data() + s.size(), v);
    if (res.ec != std::errc{} || res.ptr != s.data() + s.size()) {
        throw std::invalid_argument("malformed number in date");
    }
    return v;
}

bool contains(const std::vector<weekday>& days, weekday wd) {
    return std::ranges::find(days, wd) != days.end();
}

}  // namespace

unsigned days_in_month(year_month ym) {
    if (!ym.ok()) {
        throw std::invalid_argument("invalid year_month");
    }
    const year_month_day_last month_end{ym.year(), month_day_last{ym.month()}};
    return static_cast<unsigned>(month_end.day());
}

year_month_day parse_date(std::string_view iso) {
    if (iso.size() != 10 || iso[4] != '-' || iso[7] != '-') {
        throw std::invalid_argument("date must be YYYY-MM-DD");
    }
    const auto y = iso.substr(0, 4);
    const auto m = iso.substr(5, 2);
    const auto d = iso.substr(8, 2);
    if (!all_digits(y) || !all_digits(m) || !all_digits(d)) {
        throw std::invalid_argument("date must be YYYY-MM-DD");
    }
    const year_month_day ymd{year{static_cast<int>(parse_unsigned(y))}, month{parse_unsigned(m)},
                             day{parse_unsigned(d)}};
    if (!ymd.ok()) {
        throw std::invalid_argument("invalid calendar date");
    }
    return ymd;
}

year_month_day add_months(year_month_day d, months m) {
    if (!d.ok()) {
        throw std::invalid_argument("invalid date");
    }
    const year_month ym = year_month{d.year(), d.month()} + m;
    const year_month_day_last month_end{ym.year(), month_day_last{ym.month()}};
    return year_month_day{ym.year(), ym.month(), std::min(d.day(), month_end.day())};
}

std::vector<year_month_day> billing_dates(year_month_day anchor, int count) {
    if (count < 0) {
        throw std::invalid_argument("count must be non-negative");
    }
    if (!anchor.ok()) {
        throw std::invalid_argument("invalid anchor date");
    }
    std::vector<year_month_day> out;
    out.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        out.push_back(add_months(anchor, months{i}));  // always from the anchor: no drift
    }
    return out;
}

year_month_day last_weekday(year_month ym, weekday wd) {
    if (!ym.ok() || !wd.ok()) {
        throw std::invalid_argument("invalid year_month or weekday");
    }
    return static_cast<year_month_day>(ym / wd[std::chrono::last]);
}

BusinessCalendar::BusinessCalendar(std::vector<year_month_day> holidays, std::vector<weekday> weekend)
    : weekend_(std::move(weekend)) {
    for (const auto& h : holidays) {
        if (!h.ok()) {
            throw std::invalid_argument("invalid holiday date");
        }
        holidays_.insert(h);
    }
    const bool every_day_off = std::ranges::all_of(
        std::views::iota(0u, 7u), [this](unsigned i) { return contains(weekend_, weekday{i}); });
    if (every_day_off) {
        throw std::invalid_argument("weekend covers every weekday; no business day exists");
    }
}

bool BusinessCalendar::is_business_day(year_month_day d) const {
    if (!d.ok()) {
        throw std::invalid_argument("invalid date");
    }
    return !contains(weekend_, weekday{sys_days{d}}) && !holidays_.contains(d);
}

year_month_day BusinessCalendar::add_business_days(year_month_day d, int n) const {
    if (!d.ok()) {
        throw std::invalid_argument("invalid date");
    }
    if (n == 0) {
        return d;
    }
    const int step = n > 0 ? 1 : -1;
    int remaining = n > 0 ? n : -n;
    sys_days cur{d};
    while (remaining > 0) {
        cur += days{step};
        if (is_business_day(year_month_day{cur})) {
            --remaining;
        }
    }
    return year_month_day{cur};
}

int BusinessCalendar::business_days_between(year_month_day start, year_month_day end) const {
    if (!start.ok() || !end.ok()) {
        throw std::invalid_argument("invalid date");
    }
    if (end < start) {
        return -business_days_between(end, start);
    }
    int count = 0;
    sys_days cur{start};
    const sys_days target{end};
    while (cur < target) {
        cur += days{1};
        if (is_business_day(year_month_day{cur})) {
            ++count;
        }
    }
    return count;
}

}  // namespace cppref::chrono_time
