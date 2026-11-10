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
        if (running_) {
            throw std::logic_error("Stopwatch already running");
        }
        start_ = Clock::now();
        running_ = true;
    }

    // Returns the time since start() and stops. std::logic_error if not running.
    std::chrono::nanoseconds stop() {
        if (!running_) {
            throw std::logic_error("Stopwatch is not running");
        }
        last_ = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start_);
        running_ = false;
        return last_;
    }

    // Live reading while running; the last stopped value after stop(); zero before the first start().
    std::chrono::nanoseconds elapsed() const {
        if (running_) {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start_);
        }
        return last_;
    }

private:
    typename Clock::time_point start_{};
    std::chrono::nanoseconds last_{0};
    bool running_ = false;
};

}  // namespace cppref::chrono_time
