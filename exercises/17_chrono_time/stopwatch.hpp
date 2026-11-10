// Part 7b: elapsed-time meter over an injected steady clock.
//
// Wall clocks (system_clock) can jump with NTP or the user; a stopwatch must use a
// steady clock. The clock is a template parameter so tests can inject a fake one at
// zero runtime cost.
#pragma once

#include <chrono>
#include <concepts>
#include <stdexcept>

namespace cppref::chrono_time {

template <typename C>
concept SteadyClock = std::chrono::is_clock_v<C> && C::is_steady;

template <SteadyClock Clock = std::chrono::steady_clock>
class Stopwatch {
public:
    // std::logic_error if already running.
    void start() {
        throw std::logic_error("not implemented: Stopwatch::start");
    }

    // Returns the time since start() and stops. std::logic_error if not running.
    std::chrono::nanoseconds stop() {
        throw std::logic_error("not implemented: Stopwatch::stop");
    }

    // Live reading while running; the last stopped value after stop(); zero before the first start().
    std::chrono::nanoseconds elapsed() const {
        throw std::logic_error("not implemented: Stopwatch::elapsed");
    }

private:
    // Engineer: running flag, start time point, last stopped duration.
};

}  // namespace cppref::chrono_time
