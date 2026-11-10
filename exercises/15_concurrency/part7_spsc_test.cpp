#include "spsc_ring.hpp"
#include "test_support_test.hpp"

#include <chrono>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include <gtest/gtest.h>

using cppref::concurrency::SpscRing;
using cppref::concurrency::testing::Background;

TEST(Part7Spsc, EmptyRingPopsNothingAndReportsCapacity) {
    SpscRing<int, 8> ring;
    using Ring8 = SpscRing<int, 8>;
    EXPECT_EQ(Ring8::capacity(), 8u);
    EXPECT_FALSE(ring.try_pop().has_value());
    EXPECT_EQ(ring.size_approx(), 0u);
}

TEST(Part7Spsc, FullRingRejectsAndLeavesValueUntouched) {
    SpscRing<std::string, 2> ring;
    EXPECT_TRUE(ring.try_push(std::string("a")));
    EXPECT_TRUE(ring.try_push(std::string("b")));

    std::string c = "c";
    EXPECT_FALSE(ring.try_push(std::move(c)));
    EXPECT_EQ(c, "c");  // a failed push must not consume the argument

    EXPECT_EQ(ring.try_pop(), "a");
    EXPECT_TRUE(ring.try_push(std::move(c)));
    EXPECT_EQ(ring.try_pop(), "b");
    EXPECT_EQ(ring.try_pop(), "c");
}

TEST(Part7Spsc, FifoAcrossIndexWraparound) {
    SpscRing<int, 4> ring;
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(ring.try_push(int(i)));
        EXPECT_EQ(ring.try_pop(), i);
    }
    EXPECT_EQ(ring.size_approx(), 0u);
}

TEST(Part7Spsc, MoveOnlyPayloads) {
    SpscRing<std::unique_ptr<int>, 4> ring;
    ASSERT_TRUE(ring.try_push(std::make_unique<int>(5)));
    auto got = ring.try_pop();
    ASSERT_TRUE(got.has_value());
    ASSERT_NE(*got, nullptr);
    EXPECT_EQ(**got, 5);
}

TEST(Part7Spsc, DestructorDestroysLeftovers) {
    std::weak_ptr<int> watch;
    {
        SpscRing<std::shared_ptr<int>, 4> ring;
        auto payload = std::make_shared<int>(1);
        watch = payload;
        ASSERT_TRUE(ring.try_push(std::move(payload)));
        EXPECT_FALSE(watch.expired());
    }
    EXPECT_TRUE(watch.expired());
}

TEST(Part7Spsc, ProducerConsumerStressPreservesOrder) {
    constexpr int total = 200000;
    SpscRing<int, 64> ring;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);

    Background producer([&] {
        for (int i = 0; i < total; ++i) {
            while (!ring.try_push(int(i))) {
                if (std::chrono::steady_clock::now() > deadline) throw std::runtime_error("producer timed out");
                std::this_thread::yield();
            }
        }
    });

    Background consumer([&] {
        int expected = 0;
        while (expected < total) {
            if (auto v = ring.try_pop()) {
                if (*v != expected) throw std::runtime_error("out of order at " + std::to_string(expected));
                ++expected;
            } else {
                if (std::chrono::steady_clock::now() > deadline) throw std::runtime_error("consumer timed out");
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();
    EXPECT_EQ(ring.try_pop(), std::nullopt);
}
