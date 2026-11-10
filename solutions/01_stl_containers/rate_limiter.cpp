#include "rate_limiter.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace cppref::stl {
namespace {

using TimePoint = std::chrono::steady_clock::time_point;

bool is_live(TimePoint event, TimePoint now, std::chrono::milliseconds window) {
    return now - event < window;
}

// Events are appended in time order, so the expired ones form a prefix.
void evict_expired(std::deque<TimePoint>& events, TimePoint now, std::chrono::milliseconds window) {
    while (!events.empty() && !is_live(events.front(), now, window)) {
        events.pop_front();
    }
}

// First live event; the live events form a suffix, so binary search applies.
auto first_live(const std::deque<TimePoint>& events, TimePoint now, std::chrono::milliseconds window) {
    return std::partition_point(events.begin(), events.end(), [&](TimePoint t) { return !is_live(t, now, window); });
}

}  // namespace

SlidingWindowLimiter::SlidingWindowLimiter(std::size_t limit, std::chrono::milliseconds window, MonotonicClock clock)
    : limit_{limit}, window_{window}, clock_{std::move(clock)} {
    if (limit_ == 0) {
        throw std::invalid_argument("SlidingWindowLimiter: limit must be positive");
    }
    if (window_ <= std::chrono::milliseconds::zero()) {
        throw std::invalid_argument("SlidingWindowLimiter: window must be positive");
    }
}

bool SlidingWindowLimiter::allow(std::string_view client) {
    const TimePoint now = clock_();
    auto it = clients_.find(client);
    if (it != clients_.end()) {
        evict_expired(it->second, now, window_);
        if (it->second.size() >= limit_) return false;
    } else {
        it = clients_.try_emplace(std::string(client)).first;
    }
    it->second.push_back(now);
    return true;
}

std::size_t SlidingWindowLimiter::remaining(std::string_view client) const {
    const auto it = clients_.find(client);
    if (it == clients_.end()) return limit_;

    const TimePoint now = clock_();
    const auto live = static_cast<std::size_t>(std::distance(first_live(it->second, now, window_), it->second.end()));
    return live >= limit_ ? 0 : limit_ - live;
}

std::chrono::milliseconds SlidingWindowLimiter::retry_after(std::string_view client) const {
    const auto it = clients_.find(client);
    if (it == clients_.end()) return std::chrono::milliseconds::zero();

    const TimePoint now = clock_();
    const auto oldest_live_it = first_live(it->second, now, window_);
    const auto live = static_cast<std::size_t>(std::distance(oldest_live_it, it->second.end()));
    if (live < limit_) return std::chrono::milliseconds::zero();

    const TimePoint frees_at = *oldest_live_it + window_;
    return std::chrono::duration_cast<std::chrono::milliseconds>(frees_at - now);
}

std::size_t SlidingWindowLimiter::prune() {
    const TimePoint now = clock_();
    std::size_t dropped = 0;
    for (auto it = clients_.begin(); it != clients_.end();) {
        evict_expired(it->second, now, window_);
        if (it->second.empty()) {
            it = clients_.erase(it);
            ++dropped;
        } else {
            ++it;
        }
    }
    return dropped;
}

}  // namespace cppref::stl
