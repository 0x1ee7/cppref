#include "humanize.hpp"

#include <chrono>
#include <format>
#include <string_view>

namespace cppref::chrono_time {

using namespace std::chrono;

namespace {

std::string count_phrase(long long n, std::string_view unit) {
    return std::format("{} {}{}", n, unit, n == 1 ? "" : "s");
}

}  // namespace

std::string humanize(const ZonedInstant& when, const ZonedInstant& now) {
    const milliseconds signed_diff = when.get_sys_time() - now.get_sys_time();
    const bool future = signed_diff > milliseconds{0};
    const milliseconds diff = abs(signed_diff);

    if (diff < seconds{45}) {
        return "just now";
    }
    if (diff >= hours{24} * 30) {
        return std::format("on {:%F}", when.get_local_time());
    }

    std::string phrase;
    if (diff < hours{1}) {
        const long long n = std::max<long long>(1, duration_cast<minutes>(diff).count());
        phrase = count_phrase(n, "minute");
    } else if (diff < hours{24}) {
        phrase = count_phrase(duration_cast<hours>(diff).count(), "hour");
    } else {
        phrase = count_phrase(duration_cast<days>(diff).count(), "day");
    }
    return future ? "in " + phrase : phrase + " ago";
}

}  // namespace cppref::chrono_time
