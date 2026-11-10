#pragma once

#include <chrono>
#include <cstddef>
#include <string_view>

#include "clock.hpp"

namespace cppref::stl {

/// Per-client sliding-window limiter: at most `limit` allowed requests in any
/// trailing `window`. Denied requests are not recorded.
///
/// Semantics (the tests are the spec):
///  - An event at `t` counts at `now` iff `now - t < window`.
///  - `allow`: if fewer than `limit` live events, record `now` and return true;
///    otherwise return false and record nothing.
///  - `remaining`: `max(0, limit - live_events)`.
///  - `retry_after`: 0 while `remaining > 0`; otherwise the time until the oldest
///    live event leaves the window.
///  - `prune`: drop state for clients with no live events; return how many were dropped.
///  - Queries (`remaining`, `retry_after`) must not create state for unknown clients.
class SlidingWindowLimiter {
public:
    /// @throws std::invalid_argument if `limit == 0` or `window <= 0`.
    SlidingWindowLimiter(std::size_t limit, std::chrono::milliseconds window, MonotonicClock clock = steady_clock_source());

    /// Records and returns true if `client` is under its limit; otherwise false.
    bool allow(std::string_view client);

    /// Requests `client` may still make inside the current window.
    std::size_t remaining(std::string_view client) const;

    /// Zero if `client` may request now; otherwise time until the next slot frees.
    std::chrono::milliseconds retry_after(std::string_view client) const;

    /// Drops state for clients with no live events. Returns the number of clients dropped.
    std::size_t prune();

private:
    // Engineer: per-client time-ordered event store (std::deque of time_point),
    // a client index keyed by std::string with a transparent hash so that
    // string_view lookups do not allocate; limit, window and the clock.
};

}  // namespace cppref::stl
