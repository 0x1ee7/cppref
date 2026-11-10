#pragma once

#include <chrono>
#include <cstddef>
#include <deque>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

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
    // Hashes a std::string and a std::string_view identically, so lookups by
    // string_view never allocate.
    struct StringHash {
        using is_transparent = void;
        std::size_t operator()(std::string_view s) const noexcept { return std::hash<std::string_view>{}(s); }
    };
    using Events = std::deque<std::chrono::steady_clock::time_point>;  // oldest first

    std::size_t limit_;
    std::chrono::milliseconds window_;
    MonotonicClock clock_;
    std::unordered_map<std::string, Events, StringHash, std::equal_to<>> clients_;
};

}  // namespace cppref::stl
