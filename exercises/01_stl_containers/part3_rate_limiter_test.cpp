#include "clock.hpp"
#include "rate_limiter.hpp"

#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace {

using namespace std::chrono_literals;
using namespace std::string_view_literals;
using cppref::stl::MonotonicClock;
using cppref::stl::SlidingWindowLimiter;
using Tp = std::chrono::steady_clock::time_point;

// Hand-advanced clock; the clock() closure captures `this`.
class FakeTime {
public:
    MonotonicClock clock() {
        return [this] { return now_; };
    }
    void advance(std::chrono::milliseconds d) { now_ += d; }

private:
    Tp now_{std::chrono::steady_clock::now()};
};

}  // namespace

TEST(Part3RateLimiter, RejectsZeroLimitAndNonPositiveWindow) {
    FakeTime time;
    EXPECT_THROW(SlidingWindowLimiter(0, 10ms, time.clock()), std::invalid_argument);
    EXPECT_THROW(SlidingWindowLimiter(1, 0ms, time.clock()), std::invalid_argument);
    EXPECT_THROW(SlidingWindowLimiter(1, -1ms, time.clock()), std::invalid_argument);
}

TEST(Part3RateLimiter, AllowsUpToLimitThenDenies) {
    FakeTime time;
    SlidingWindowLimiter limiter(3, 1000ms, time.clock());

    EXPECT_TRUE(limiter.allow("acme"));
    EXPECT_TRUE(limiter.allow("acme"));
    EXPECT_TRUE(limiter.allow("acme"));
    EXPECT_FALSE(limiter.allow("acme"));
    EXPECT_EQ(limiter.remaining("acme"), 0u);
}

TEST(Part3RateLimiter, DeniedRequestsAreNotRecorded) {
    FakeTime time;
    SlidingWindowLimiter limiter(1, 10ms, time.clock());
    EXPECT_TRUE(limiter.allow("acme"));  // event at t=0

    time.advance(5ms);
    EXPECT_FALSE(limiter.allow("acme"));  // denied at t=5, must not extend anything

    time.advance(5ms);  // t=10: the event at t=0 is exactly one window old, so it has left
    EXPECT_TRUE(limiter.allow("acme"));
}

TEST(Part3RateLimiter, RetryAfterCountsDownToOldestEvent) {
    FakeTime time;
    SlidingWindowLimiter limiter(2, 100ms, time.clock());
    EXPECT_TRUE(limiter.allow("acme"));  // t=0
    EXPECT_EQ(limiter.retry_after("acme").count(), 0);
    time.advance(30ms);
    EXPECT_TRUE(limiter.allow("acme"));  // t=30

    time.advance(10ms);  // t=40: full
    EXPECT_FALSE(limiter.allow("acme"));
    EXPECT_EQ(limiter.remaining("acme"), 0u);
    EXPECT_EQ(limiter.retry_after("acme").count(), 60);  // oldest (t=0) leaves at t=100
}

TEST(Part3RateLimiter, ClientsAreIsolatedAndStringViewKeysShareState) {
    FakeTime time;
    SlidingWindowLimiter limiter(1, 1000ms, time.clock());
    const std::string acme = "acme";

    EXPECT_TRUE(limiter.allow(acme));       // std::string key
    EXPECT_FALSE(limiter.allow("acme"sv));  // same client via string_view
    EXPECT_TRUE(limiter.allow("globex"));   // other client unaffected
    EXPECT_EQ(limiter.remaining("acme"sv), 0u);
}

TEST(Part3RateLimiter, PruneDropsOnlyClientsWithNoLiveEvents) {
    FakeTime time;
    SlidingWindowLimiter limiter(1, 100ms, time.clock());
    EXPECT_TRUE(limiter.allow("idle"));  // t=0
    time.advance(50ms);
    EXPECT_TRUE(limiter.allow("busy"));  // t=50
    time.advance(70ms);                  // t=120: idle's event is 120ms old, busy's 70ms

    EXPECT_EQ(limiter.prune(), 1u);
    EXPECT_EQ(limiter.remaining("busy"), 0u);
    EXPECT_EQ(limiter.remaining("idle"), 1u);
}

TEST(Part3RateLimiter, QueriesDoNotCreateState) {
    FakeTime time;
    SlidingWindowLimiter limiter(5, 100ms, time.clock());

    EXPECT_EQ(limiter.remaining("ghost"sv), 5u);
    EXPECT_EQ(limiter.retry_after("ghost"sv).count(), 0);

    EXPECT_EQ(limiter.prune(), 0u);
}

TEST(Part3RateLimiter, RemainingIgnoresExpiredEvents) {
    FakeTime time;
    SlidingWindowLimiter limiter(2, 10ms, time.clock());
    EXPECT_TRUE(limiter.allow("acme"));
    EXPECT_TRUE(limiter.allow("acme"));

    time.advance(10ms);

    EXPECT_EQ(limiter.remaining("acme"), 2u);
    EXPECT_TRUE(limiter.allow("acme"));
}
