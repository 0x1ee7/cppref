#include "queue.hpp"
#include "test_support_test.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <optional>
#include <set>
#include <stdexcept>
#include <stop_token>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using cppref::concurrency::BoundedQueue;
using cppref::concurrency::testing::Background;
using cppref::concurrency::testing::parallel_for;
using cppref::concurrency::testing::TimedLatch;

TEST(Part5Queue, FifoOrder) {
    BoundedQueue<int> queue(4);
    EXPECT_TRUE(queue.push(1));
    EXPECT_TRUE(queue.push(2));
    EXPECT_TRUE(queue.push(3));
    EXPECT_EQ(queue.size(), 3u);
    EXPECT_EQ(queue.pop(), 1);
    EXPECT_EQ(queue.pop(), 2);
    EXPECT_EQ(queue.pop(), 3);
    EXPECT_EQ(queue.capacity(), 4u);
}

TEST(Part5Queue, ZeroCapacityRejected) {
    EXPECT_THROW(BoundedQueue<int>(0), std::invalid_argument);
}

TEST(Part5Queue, PushBlocksWhenFullUntilSpaceFrees) {
    BoundedQueue<int> queue(1);
    ASSERT_TRUE(queue.push(1));

    TimedLatch pushed(1);
    Background producer([&] {
        EXPECT_TRUE(queue.push(2));
        pushed.count_down();
    });

    // The producer must still be blocked: the queue is full.
    EXPECT_FALSE(pushed.wait_for(std::chrono::milliseconds(50)));

    EXPECT_EQ(queue.pop(), 1);
    EXPECT_TRUE(pushed.wait_for());
    producer.join();
    EXPECT_EQ(queue.pop(), 2);
}

TEST(Part5Queue, CloseDrainsBufferedItemsThenReturnsNullopt) {
    BoundedQueue<int> queue(4);
    ASSERT_TRUE(queue.push(1));
    ASSERT_TRUE(queue.push(2));
    queue.close();

    EXPECT_TRUE(queue.closed());
    EXPECT_FALSE(queue.push(3));
    EXPECT_EQ(queue.pop(), 1);
    EXPECT_EQ(queue.pop(), 2);
    EXPECT_EQ(queue.pop(), std::nullopt);
}

TEST(Part5Queue, CloseWakesBlockedConsumer) {
    BoundedQueue<int> queue(2);
    TimedLatch done(1);
    std::optional<int> received = 42;

    Background consumer([&] {
        received = queue.pop();
        done.count_down();
    });

    queue.close();
    EXPECT_TRUE(done.wait_for());
    consumer.join();
    EXPECT_EQ(received, std::nullopt);
}

TEST(Part5Queue, CloseWakesBlockedProducer) {
    BoundedQueue<int> queue(1);
    ASSERT_TRUE(queue.push(1));

    std::atomic<bool> accepted{true};
    TimedLatch done(1);
    Background producer([&] {
        accepted = queue.push(2);
        done.count_down();
    });

    queue.close();
    EXPECT_TRUE(done.wait_for());
    producer.join();
    EXPECT_FALSE(accepted.load());
}

TEST(Part5Queue, StopWakesBlockedPop) {
    BoundedQueue<int> queue(2);
    std::stop_source source;
    TimedLatch done(1);
    std::optional<int> received = 42;

    Background consumer([&] {
        received = queue.pop(source.get_token());
        done.count_down();
    });

    source.request_stop();
    EXPECT_TRUE(done.wait_for());
    consumer.join();
    EXPECT_EQ(received, std::nullopt);
    EXPECT_FALSE(queue.closed());
}

TEST(Part5Queue, StopWakesBlockedPushWithoutLosingData) {
    BoundedQueue<int> queue(1);
    ASSERT_TRUE(queue.push(1));

    std::stop_source source;
    TimedLatch done(1);
    std::atomic<bool> accepted{true};
    Background producer([&] {
        accepted = queue.push(2, source.get_token());
        done.count_down();
    });

    source.request_stop();
    EXPECT_TRUE(done.wait_for());
    producer.join();
    EXPECT_FALSE(accepted.load());
    EXPECT_EQ(queue.pop(), 1);
    EXPECT_EQ(queue.size(), 0u);
}

TEST(Part5Queue, MpmcStressDeliversEveryItemExactlyOnce) {
    constexpr std::size_t producers = 4;
    constexpr int per_producer = 1000;
    constexpr std::size_t consumers = 4;

    BoundedQueue<int> queue(8);
    std::vector<std::vector<int>> received(consumers);

    // Producers run on a driver thread; it closes the queue once they are all done.
    Background driver([&] {
        parallel_for(producers, [&](std::size_t p) {
            for (int i = 0; i < per_producer; ++i) {
                ASSERT_TRUE(queue.push(static_cast<int>(p) * per_producer + i));
            }
        });
        queue.close();
    });

    parallel_for(consumers, [&](std::size_t c) {
        while (auto item = queue.pop()) received[c].push_back(*item);
    });
    driver.join();

    std::set<int> seen;
    std::size_t total = 0;
    for (const auto& bucket : received) {
        total += bucket.size();
        seen.insert(bucket.begin(), bucket.end());
    }
    EXPECT_EQ(total, producers * per_producer);
    EXPECT_EQ(seen.size(), producers * per_producer);
}
