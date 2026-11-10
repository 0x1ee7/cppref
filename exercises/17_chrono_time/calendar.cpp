#include "calendar.hpp"

#include <stdexcept>

namespace cppref::chrono_time {

using std::chrono::months;
using std::chrono::weekday;
using std::chrono::year_month;
using std::chrono::year_month_day;

unsigned days_in_month(year_month) {
    throw std::logic_error("not implemented: days_in_month");
}

year_month_day parse_date(std::string_view) {
    throw std::logic_error("not implemented: parse_date");
}

year_month_day add_months(year_month_day, months) {
    throw std::logic_error("not implemented: add_months");
}

std::vector<year_month_day> billing_dates(year_month_day, int) {
    throw std::logic_error("not implemented: billing_dates");
}

year_month_day last_weekday(year_month, weekday) {
    throw std::logic_error("not implemented: last_weekday");
}

BusinessCalendar::BusinessCalendar(std::vector<year_month_day>, std::vector<weekday>) {
    throw std::logic_error("not implemented: BusinessCalendar");
}

bool BusinessCalendar::is_business_day(year_month_day) const {
    throw std::logic_error("not implemented: is_business_day");
}

year_month_day BusinessCalendar::add_business_days(year_month_day, int) const {
    throw std::logic_error("not implemented: add_business_days");
}

int BusinessCalendar::business_days_between(year_month_day, year_month_day) const {
    throw std::logic_error("not implemented: business_days_between");
}

}  // namespace cppref::chrono_time
