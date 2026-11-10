#include "futures.hpp"
#include "test_support_test.hpp"
#include "thread_pool.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <future>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using cppref::concurrency::run_all;
using cppref::concurrency::ShutdownMode;
using cppref::concurrency::ThreadPool;
using cppref::concurrency::testing::Background;
using cppref::concurrency::testing::TimedLatch;

TEST(Part6Pool, ZeroThreadsRejected) {
    EXPECT_THROW(ThreadPool(0), std::invalid_argument);
}

TEST(Part6Pool, SubmitReturnsResultThroughFuture) {
    ThreadPool pool(2);
    auto future = pool.submit([] { return 6 * 7; });
    EXPECT_EQ(future.get(), 42);
}

TEST(Part6Pool, TaskExceptionPropagatesThroughFuture) {
    ThreadPool pool(2);
    auto future = pool.submit([]() -> int { throw std::runtime_error("task failed"); });
    try {
        (void)future.get();
        FAIL() << "expected the task exception";
    } catch (const std::runtime_error& e) {
        EXPECT_EQ(std::string(e.what()), "task failed");
    }
}

TEST(Part6Pool, TasksRunConcurrently) {
    constexpr std::size_t n = 4;
    ThreadPool pool(n);
    TimedLatch all_started(static_cast<std::ptrdiff_t>(n));

    // Each task waits until all four are running; with serial execution this would time out.
    std::vector<std::future<bool>> futures;
    for (std::size_t i = 0; i < n; ++i) {
        futures.push_back(pool.submit([&all_started] {
            all_started.count_down();
            return all_started.wait_for();
        }));
    }
    for (auto& f : futures) EXPECT_TRUE(f.get());
}

TEST(Part6Pool, VoidTasksRunAndDestructorDrains) {
    std::atomic<int> count{0};
    {
        ThreadPool pool(2);
        for (int i = 0; i < 20; ++i) {
            (void)pool.submit([&count] { ++count; });
        }
        // Destructor = shutdown(drain): every queued task must finish before it returns.
    }
    EXPECT_EQ(count.load(), 20);
}

TEST(Part6Shutdown, DrainRunsEverythingQueued) {
    ThreadPool pool(1);
    std::atomic<int> count{0};
    for (int i = 0; i < 5; ++i) {
        (void)pool.submit([&count] { ++count; });
    }
    pool.shutdown(ShutdownMode::drain);
    EXPECT_EQ(count.load(), 5);
    EXPECT_EQ(pool.pending(), 0u);
}

TEST(Part6Shutdown, IdempotentAndSubmitAfterShutdownRejected) {
    ThreadPool pool(2);
    pool.shutdown();
    EXPECT_NO_THROW(pool.shutdown(ShutdownMode::cancel));  // second call is a no-op
    EXPECT_THROW((void)pool.submit([] {}), std::runtime_error);
}

TEST(Part6Shutdown, CancelDropsQueuedWork) {
    ThreadPool pool(1);
    TimedLatch started(1);
    TimedLatch gate(1);

    auto running = pool.submit([&] {
        started.count_down();
        return gate.wait_for();
    });
    ASSERT_TRUE(started.wait_for());

    std::atomic<int> ran{0};
    std::vector<std::future<void>> queued;
    for (int i = 0; i < 5; ++i) {
        queued.push_back(pool.submit([&ran] { ++ran; }));
    }
    EXPECT_EQ(pool.pending(), 5u);

    // Cancel from another thread: shutdown joins the blocked worker, so the gate opens afterwards.
    Background canceller([&pool] { pool.shutdown(ShutdownMode::cancel); });
    for (int i = 0; i < 5000 && pool.pending() != 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    EXPECT_EQ(pool.pending(), 0u);
    gate.count_down();
    canceller.join();

    EXPECT_TRUE(running.get());
    EXPECT_EQ(ran.load(), 0);
    for (auto& f : queued) {
        EXPECT_THROW(f.get(), std::future_error);
    }
}

TEST(Part6RunAll, ResultsKeepInputOrder) {
    std::vector<std::function<int()>> jobs;
    for (int i = 0; i < 6; ++i) {
        jobs.push_back([i] {
            std::this_thread::sleep_for(std::chrono::milliseconds((6 - i) * 3));
            return i * i;
        });
    }
    const auto results = run_all(std::move(jobs));
    EXPECT_EQ(results, (std::vector<int>{0, 1, 4, 9, 16, 25}));
}

TEST(Part6RunAll, EmptyInputReturnsEmpty) {
    EXPECT_TRUE(run_all(std::vector<std::function<int()>>{}).empty());
}

TEST(Part6RunAll, JobsRunConcurrently) {
    TimedLatch all_started(3);
    std::vector<std::function<bool()>> jobs;
    for (int i = 0; i < 3; ++i) {
        jobs.push_back([&all_started] {
            all_started.count_down();
            return all_started.wait_for();
        });
    }
    for (bool ok : run_all(std::move(jobs))) EXPECT_TRUE(ok);
}

TEST(Part6RunAll, FirstFailureRethrownAfterAllFinish) {
    std::atomic<bool> slow_finished{false};
    std::vector<std::function<int()>> jobs;
    jobs.push_back([&slow_finished] {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        slow_finished = true;
        return 0;
    });
    jobs.push_back([]() -> int { throw std::runtime_error("first"); });
    jobs.push_back([]() -> int { throw std::runtime_error("second"); });

    try {
        (void)run_all(std::move(jobs));
        FAIL() << "expected run_all to throw";
    } catch (const std::runtime_error& e) {
        EXPECT_EQ(std::string(e.what()), "first");
    }
    EXPECT_TRUE(slow_finished.load());
}
