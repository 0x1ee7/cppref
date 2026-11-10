#include "meetings.hpp"

#include "zones.hpp"

#include <stdexcept>

namespace cppref::chrono_time {

using namespace std::chrono;

std::vector<Instant> weekly_occurrences(const WeeklyRule& rule) {
    if (!rule.start.ok()) {
        throw std::invalid_argument("invalid start date");
    }
    if (rule.at < minutes{0} || rule.at >= hours{24}) {
        throw std::invalid_argument("'at' must be within [0h, 24h)");
    }
    const time_zone* zone = find_zone(rule.zone);

    std::vector<Instant> out;
    out.reserve(rule.count);

    // First date on or after start that falls on rule.day, then every 7 days.
    sys_days day = sys_days{rule.start};
    day += rule.day - weekday{day};

    while (out.size() < rule.count) {
        const local_time<milliseconds> wall = static_cast<local_days>(year_month_day{day}) + rule.at;
        const local_info info = zone->get_info(wall);
        const Instant wall_as_utc{wall.time_since_epoch()};

        switch (info.result) {
            case local_info::unique:
                out.push_back(wall_as_utc - info.first.offset);
                break;
            case local_info::nonexistent:
                if (rule.gap == Gap::shift_forward) {
                    // Using the pre-transition offset lands on the instant the clock jumps to.
                    out.push_back(wall_as_utc - info.first.offset);
                }
                break;
            case local_info::ambiguous:
                if (rule.overlap == Overlap::earlier) {
                    out.push_back(wall_as_utc - info.first.offset);
                } else if (rule.overlap == Overlap::later) {
                    out.push_back(wall_as_utc - info.second.offset);
                }
                break;
        }
        day += days{7};
    }
    return out;
}

}  // namespace cppref::chrono_time
