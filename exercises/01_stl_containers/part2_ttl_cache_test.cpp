#include "clock.hpp"
#include "ttl_cache.hpp"

#include <chrono>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {

using namespace std::chrono_literals;
using cppref::stl::MonotonicClock;
using cppref::stl::TtlCache;
using Tp = std::chrono::steady_clock::time_point;
using Cache = TtlCache<std::string, int>;
using Keys = std::vector<std::string>;

// Hand-advanced clock. The clock() closure captures `this`: keep FakeTime alive
// for as long as any cache built from it.
class FakeTime {
public:
    MonotonicClock clock() {
        return [this] { return now_; };
    }
    void advance(std::chrono::milliseconds d) { now_ += d; }

private:
    Tp now_{std::chrono::steady_clock::now()};
};

struct MockClock {
    MOCK_METHOD(Tp, now, ());
};

}  // namespace

TEST(Part2TtlCache, RejectsZeroCapacityAndNonPositiveTtl) {
    FakeTime time;
    EXPECT_THROW(Cache(0, 10ms, time.clock()), std::invalid_argument);
    EXPECT_THROW(Cache(1, 0ms, time.clock()), std::invalid_argument);
    EXPECT_THROW(Cache(1, -1ms, time.clock()), std::invalid_argument);
}

TEST(Part2TtlCache, GetCountsHitsAndMisses) {
    FakeTime time;
    Cache cache(4, 100ms, time.clock());

    EXPECT_EQ(cache.get("missing"), std::nullopt);
    cache.put("a", 1);
    EXPECT_EQ(cache.get("a"), 1);

    EXPECT_EQ(cache.hits(), 1u);
    EXPECT_EQ(cache.misses(), 1u);
}

TEST(Part2TtlCache, EntryIsAMissAtExactlyItsExpiryInstant) {
    FakeTime time;
    Cache cache(4, 100ms, time.clock());
    cache.put("a", 1);

    time.advance(99ms);
    EXPECT_EQ(cache.get("a"), 1);

    time.advance(1ms);  // exactly ttl after the write
    EXPECT_EQ(cache.get("a"), std::nullopt);
    EXPECT_EQ(cache.misses(), 1u);
}

TEST(Part2TtlCache, GetDoesNotExtendTtl) {
    FakeTime time;
    Cache cache(4, 100ms, time.clock());
    cache.put("a", 1);

    time.advance(60ms);
    EXPECT_EQ(cache.get("a"), 1);  // hit refreshes recency, not expiry
    time.advance(40ms);            // now 100ms after the write

    EXPECT_EQ(cache.get("a"), std::nullopt);
}

TEST(Part2TtlCache, PutReplacementRefreshesTtlAndValue) {
    FakeTime time;
    Cache cache(4, 100ms, time.clock());
    cache.put("a", 1);

    time.advance(60ms);
    cache.put("a", 2);  // new expiry: now + 100ms
    time.advance(60ms);  // 120ms after the first write, 60ms after the second

    EXPECT_EQ(cache.get("a"), 2);
}

TEST(Part2TtlCache, EvictsLeastRecentlyUsedAndKeysAreLruFirst) {
    FakeTime time;
    Cache cache(2, 1000ms, time.clock());
    cache.put("a", 1);
    cache.put("b", 2);
    EXPECT_EQ(cache.get("a"), 1);  // a becomes most recently used

    cache.put("c", 3);  // over capacity: b is the LRU and goes

    EXPECT_FALSE(cache.contains("b"));
    EXPECT_EQ(cache.keys(), (Keys{"a", "c"}));
}

TEST(Part2TtlCache, ExpiredEntriesDoNotCountTowardCapacityAndSizeCountsLiveOnly) {
    FakeTime time;
    Cache cache(2, 100ms, time.clock());
    cache.put("a", 1);  // expires at +100
    time.advance(50ms);
    cache.put("b", 2);  // expires at +150
    time.advance(60ms);  // a is expired, b is live

    cache.put("c", 3);  // purge a first, so b must NOT be evicted

    EXPECT_TRUE(cache.contains("b"));
    EXPECT_TRUE(cache.contains("c"));
    EXPECT_EQ(cache.size(), 2u);

    time.advance(100ms);  // everything expired
    EXPECT_EQ(cache.size(), 0u);
    EXPECT_TRUE(cache.keys().empty());
}

TEST(Part2TtlCache, ContainsAndKeysDoNotChangeRecencyOrCounters) {
    FakeTime time;
    Cache cache(4, 100ms, time.clock());
    cache.put("a", 1);
    cache.put("b", 2);

    EXPECT_TRUE(cache.contains("a"));  // must not promote a
    EXPECT_EQ(cache.keys(), (Keys{"a", "b"}));
    EXPECT_EQ(cache.hits(), 0u);
    EXPECT_EQ(cache.misses(), 0u);

    time.advance(100ms);
    EXPECT_FALSE(cache.contains("a"));
}

TEST(Part2TtlCache, ReadsTimeOnlyThroughInjectedClock) {
    MockClock clock;
    const Tp t0 = std::chrono::steady_clock::now();
    EXPECT_CALL(clock, now()).Times(::testing::AtLeast(1)).WillRepeatedly(::testing::Return(t0));

    Cache cache(2, 100ms, [&clock] { return clock.now(); });
    cache.put("a", 1);

    EXPECT_EQ(cache.get("a"), 1);
}
