#include "cache.hpp"
#include "test_support_test.hpp"

#include <atomic>
#include <thread>
#include <chrono>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

using cppref::concurrency::BoundedCache;
using cppref::concurrency::SingleFlightCache;
using cppref::concurrency::testing::Background;
using cppref::concurrency::testing::parallel_for;
using cppref::concurrency::testing::TimedLatch;

TEST(Part4Lru, GetAndPutRoundTripAndCountHitsAndMisses) {
    BoundedCache<std::string, int> cache(2);
    cache.put("a", 1);
    cache.put("b", 2);

    EXPECT_EQ(cache.get("a"), 1);
    EXPECT_FALSE(cache.get("missing").has_value());
    EXPECT_EQ(cache.hits(), 1u);
    EXPECT_EQ(cache.misses(), 1u);
}

TEST(Part4Lru, EvictsLeastRecentlyUsed) {
    BoundedCache<std::string, int> cache(2);
    cache.put("a", 1);
    cache.put("b", 2);
    EXPECT_TRUE(cache.get("a").has_value());  // "a" becomes most recent

    cache.put("c", 3);  // evicts "b"
    EXPECT_FALSE(cache.contains("b"));
    EXPECT_TRUE(cache.contains("a"));
    EXPECT_TRUE(cache.contains("c"));
    EXPECT_EQ(cache.size(), 2u);
}

TEST(Part4Lru, ContainsHasNoSideEffects) {
    BoundedCache<std::string, int> cache(2);
    cache.put("a", 1);
    cache.put("b", 2);

    EXPECT_TRUE(cache.contains("a"));  // must NOT refresh "a"
    EXPECT_EQ(cache.hits(), 0u);
    EXPECT_EQ(cache.misses(), 0u);

    cache.put("c", 3);  // "a" is still least recent, so it is evicted
    EXPECT_FALSE(cache.contains("a"));
    EXPECT_TRUE(cache.contains("b"));
}

TEST(Part4Lru, PutUpdatesExistingKeyWithoutGrowing) {
    BoundedCache<std::string, int> cache(2);
    cache.put("a", 1);
    cache.put("a", 2);
    EXPECT_EQ(cache.size(), 1u);
    EXPECT_EQ(cache.get("a"), 2);
}

TEST(Part4Lru, ZeroCapacityRejected) {
    EXPECT_THROW((BoundedCache<std::string, int>(0)), std::invalid_argument);
    EXPECT_THROW((SingleFlightCache<int, int>(0)), std::invalid_argument);
}

TEST(Part4Lru, ContendedUseKeepsCountsConsistent) {
    BoundedCache<int, int> cache(16);
    parallel_for(4, [&](std::size_t lane) {
        for (int i = 0; i < 500; ++i) {
            const int key = static_cast<int>(i % 8) + static_cast<int>(lane) * 8;
            cache.put(key, i);
            (void)cache.get(key);
        }
    });
    EXPECT_LE(cache.size(), 16u);
    EXPECT_EQ(cache.hits() + cache.misses(), 2000u);
}

TEST(Part4SingleFlight, CachedValueSkipsLoader) {
    SingleFlightCache<int, int> cache(4);
    std::atomic<int> loads{0};
    auto loader = [&](const int& key) {
        ++loads;
        return key * 10;
    };

    EXPECT_EQ(cache.get_or_load(1, loader), 10);
    EXPECT_EQ(cache.get_or_load(1, loader), 10);
    EXPECT_EQ(loads.load(), 1);
    EXPECT_EQ(cache.load_count(), 1u);
}

TEST(Part4SingleFlight, ConcurrentMissesShareOneLoad) {
    SingleFlightCache<int, int> cache(4);
    std::atomic<int> loads{0};
    std::function<int(const int&)> loader = [&](const int& key) {
        ++loads;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return key + 1;
    };

    parallel_for(8, [&](std::size_t) { EXPECT_EQ(cache.get_or_load(7, loader), 8); });
    EXPECT_EQ(loads.load(), 1);
}

TEST(Part4SingleFlight, FailureFansOutAndIsNotCached) {
    SingleFlightCache<int, int> cache(4);
    std::function<int(const int&)> failing = [](const int&) -> int {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        throw std::runtime_error("upstream down");
    };

    // Every caller fails whether it joined the in-flight load or started its own.
    std::atomic<int> failures{0};
    parallel_for(4, [&](std::size_t) {
        try {
            (void)cache.get_or_load(3, failing);
        } catch (const std::runtime_error&) {
            ++failures;
        }
    });
    EXPECT_EQ(failures.load(), 4);

    // The failure was not cached: the next call runs the loader again and succeeds.
    std::atomic<int> loads{0};
    auto ok_loader = [&](const int& key) {
        ++loads;
        return key * 2;
    };
    EXPECT_EQ(cache.get_or_load(3, ok_loader), 6);
    EXPECT_EQ(loads.load(), 1);
}

TEST(Part4SingleFlight, SlowKeyDoesNotBlockOtherKeys) {
    SingleFlightCache<int, int> cache(4);
    TimedLatch started(1);
    TimedLatch release(1);

    Background slow([&] {
        (void)cache.get_or_load(1, [&](const int& key) {
            started.count_down();
            release.wait_for(std::chrono::milliseconds(5000));
            return key;
        });
    });

    ASSERT_TRUE(started.wait_for());
    const auto begin = std::chrono::steady_clock::now();
    EXPECT_EQ(cache.get_or_load(2, [](const int& key) { return key * 100; }), 200);
    const auto elapsed = std::chrono::steady_clock::now() - begin;
    EXPECT_LT(elapsed, std::chrono::seconds(2));

    release.count_down();
    slow.join();
}

TEST(Part4SingleFlight, LruEvictsAtCapacity) {
    SingleFlightCache<int, int> cache(2);
    std::atomic<int> loads{0};
    auto loader = [&](const int& key) {
        ++loads;
        return key;
    };

    (void)cache.get_or_load(1, loader);
    (void)cache.get_or_load(2, loader);
    (void)cache.get_or_load(3, loader);  // evicts 1, the least recently used
    EXPECT_EQ(loads.load(), 3);

    (void)cache.get_or_load(1, loader);  // must load again
    EXPECT_EQ(loads.load(), 4);
    EXPECT_EQ(cache.load_count(), 4u);
}
