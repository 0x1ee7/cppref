#include "scheduler.hpp"
#include "test_support_test.hpp"

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

using cppref::concurrency::JobError;
using cppref::concurrency::PeriodicScheduler;
using cppref::concurrency::testing::TimedLatch;

using namespace std::chrono_literals;

TEST(Part7Scheduler, RejectsBadRegistration) {
    PeriodicScheduler scheduler;
    EXPECT_THROW(scheduler.add("zero", 0ms, [] {}), std::invalid_argument);
    EXPECT_THROW(scheduler.add("negative", -5ms, [] {}), std::invalid_argument);

    scheduler.add("tick", 10ms, [] {});
    EXPECT_THROW(scheduler.add("tick", 10ms, [] {}), std::invalid_argument);
}

TEST(Part7Scheduler, LifecycleErrorsAndIdempotentStop) {
    PeriodicScheduler scheduler;
    EXPECT_NO_THROW(scheduler.stop());  // no-op before start

    scheduler.add("tick", 10ms, [] {});
    scheduler.start();
    EXPECT_THROW(scheduler.start(), std::logic_error);
    EXPECT_THROW(scheduler.add("late", 10ms, [] {}), std::logic_error);

    scheduler.stop();
    EXPECT_NO_THROW(scheduler.stop());
    EXPECT_THROW(scheduler.start(), std::logic_error);
}

TEST(Part7Scheduler, RunsJobsRepeatedly) {
    TimedLatch ticks(3);
    PeriodicScheduler scheduler;
    scheduler.add("tick", 5ms, [&ticks] { ticks.count_down(); });
    scheduler.start();

    EXPECT_TRUE(ticks.wait_for());
    scheduler.stop();
}

TEST(Part7Scheduler, StopDoesNotWaitForTheInterval) {
    PeriodicScheduler scheduler;
    scheduler.add("hourly", std::chrono::hours(1), [] {});
    scheduler.start();

    const auto begin = std::chrono::steady_clock::now();
    scheduler.stop();
    EXPECT_LT(std::chrono::steady_clock::now() - begin, 2s);
}

TEST(Part7Scheduler, JobFailuresAreRecordedAndJobKeepsRunning) {
    TimedLatch attempts(2);
    PeriodicScheduler scheduler;
    scheduler.add("flaky", 5ms, [&attempts] {
        attempts.count_down();
        throw std::runtime_error("bad");
    });
    scheduler.start();

    EXPECT_TRUE(attempts.wait_for());
    scheduler.stop();

    const auto errors = scheduler.errors();
    ASSERT_GE(errors.size(), 2u);
    for (const auto& e : errors) {
        EXPECT_EQ(e, (JobError{"flaky", "bad"}));
    }
}
