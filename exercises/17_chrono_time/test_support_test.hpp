// Shared helpers for the *_test.cpp files. Pure-std constructors only, so they work
// with the stubs; anything that reaches the implementation goes through the API.
#pragma once

#include "common.hpp"
#include "zones.hpp"

#include <chrono>
#include <string_view>

namespace cppref::chrono_time::fixture {

// Instant at the given UTC wall fields.
inline Instant utc(int y, unsigned m, unsigned d, int h = 0, int mi = 0, int s = 0) {
    const std::chrono::year_month_day ymd{std::chrono::year{y}, std::chrono::month{m}, std::chrono::day{d}};
    return std::chrono::sys_days{ymd} + std::chrono::hours{h} + std::chrono::minutes{mi} +
           std::chrono::seconds{s};
}

// Local wall time (no zone) at the given fields.
inline std::chrono::local_time<std::chrono::milliseconds> wall(int y, unsigned m, unsigned d, int h = 0,
                                                               int mi = 0) {
    const std::chrono::year_month_day ymd{std::chrono::year{y}, std::chrono::month{m}, std::chrono::day{d}};
    return static_cast<std::chrono::local_days>(ymd) + std::chrono::hours{h} + std::chrono::minutes{mi};
}

// Instant displayed in a zone.
inline ZonedInstant zoned(std::string_view zone, Instant instant) {
    return ZonedInstant{find_zone(zone), instant};
}

}  // namespace cppref::chrono_time::fixture
