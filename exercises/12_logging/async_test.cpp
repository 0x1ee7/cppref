// Part 4: AsyncSink delivery, back-pressure and drops, and thread safety of the
// formatting path. Plus one Part5 test: MDC is captured at log time, not at
// delivery time. Waits use bounded timeouts so a broken implementation fails
// instead of hanging.
#include "async.hpp"
#include "logging.hpp"
#include "sinks.hpp"
#include "test_support_test.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace {

using cppref::logging::AsyncSink;
using cppref::logging::Field;
using cppref::logging::Level;
using cppref::logging::Logger;
using cppref::logging::MdcScope;
using cppref::logging::OstreamSink;
using cppref::logging::Record;
using cppref::logging::Registry;
using cppref::logging::Sink;
using cppref::logging::testing::CaptureSink;
using cppref::logging::testing::make_record;
using cppref::logging::testing::split_lines;

// Blocks inside write() until open(); lets tests fill the queue deterministically.
class GateSink final : public Sink {
public:
    void write(const Record& record) override {
        std::unique_lock lock(mutex_);
        ++entered_;
        cv_.notify_all();
        cv_.wait(lock, [this] { return open_; });
        records_.push_back(record);
    }

    void flush() override {}

    void open() {
        {
            std::scoped_lock lock(mutex_);
            open_ = true;
        }
        cv_.notify_all();
    }

    // True once at least `n` writes have started (bounded wait).
    bool wait_entered(int n) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, 5s, [&] { return entered_ >= n; });
    }

    std::vector<Record> records() {
        std::scoped_lock lock(mutex_);
        return records_;
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    bool open_ = false;
    int entered_ = 0;
    std::vector<Record> records_;
};

// Every write throws; used to check failure accounting.
class ThrowingSink final : public Sink {
public:
    void write(const Record&) override {
        throw std::runtime_error("target write failed");
    }
    void flush() override {}
};

std::shared_ptr<CaptureSink> capture() {
    return std::make_shared<CaptureSink>();
}

}  // namespace

// ---------------------------------------------------------------------------
// Part 4: AsyncSink and concurrent logging
// ---------------------------------------------------------------------------

TEST(Part4Async, DeliversAcceptedRecordsInOrderAfterFlush) {
    auto target = capture();
    AsyncSink async(target, 100);
    for (int i = 0; i < 50; ++i) {
        async.write(make_record(Level::info, "app", std::format("m{}", i)));
    }

    async.flush();

    auto records = target->records();
    ASSERT_EQ(records.size(), 50u);
    for (int i = 0; i < 50; ++i) {
        EXPECT_EQ(records[i].message, std::format("m{}", i));
    }
    EXPECT_EQ(async.dropped(), 0u);
}

TEST(Part4Async, DestructionDrainsQueueBeforeReturning) {
    auto target = capture();
    {
        AsyncSink async(target, 100);
        for (int i = 0; i < 20; ++i) {
            async.write(make_record(Level::info, "app", std::format("m{}", i)));
        }
    }
    EXPECT_EQ(target->records().size(), 20u);
}

TEST(Part4Async, FullQueueDropsNewRecordsAndCountsThem) {
    auto gate = std::make_shared<GateSink>();
    AsyncSink async(gate, 4);

    // Worker takes m0 and blocks inside the gate, so the queue is empty.
    async.write(make_record(Level::info, "app", "m0"));
    ASSERT_TRUE(gate->wait_entered(1));

    // Four more fill the queue; the next three are dropped.
    for (int i = 1; i <= 7; ++i) {
        async.write(make_record(Level::info, "app", std::format("m{}", i)));
    }
    EXPECT_EQ(async.dropped(), 3u);

    gate->open();
    async.flush();

    auto records = gate->records();
    ASSERT_EQ(records.size(), 5u);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(records[i].message, std::format("m{}", i));
    }
}

TEST(Part4Async, FlushWaitsForTheRecordInFlight) {
    auto gate = std::make_shared<GateSink>();
    AsyncSink async(gate, 8);
    async.write(make_record(Level::info, "app", "slow"));
    ASSERT_TRUE(gate->wait_entered(1));

    std::atomic<bool> flushed{false};
    std::thread flusher([&] {
        async.flush();
        flushed = true;
    });

    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(flushed.load()) << "flush() returned while a record was still in flight";

    gate->open();
    flusher.join();
    EXPECT_TRUE(flushed.load());
    EXPECT_EQ(gate->records().size(), 1u);
}

TEST(Part4Async, TargetExceptionsAreCountedNotPropagated) {
    AsyncSink async(std::make_shared<ThrowingSink>(), 16);
    for (int i = 0; i < 3; ++i) {
        EXPECT_NO_THROW(async.write(make_record(Level::info, "app", "x")));
    }
    EXPECT_NO_THROW(async.flush());

    EXPECT_EQ(async.failed(), 3u);
    EXPECT_EQ(async.dropped(), 0u);
}

TEST(Part4Async, RejectsZeroCapacityAndNullTarget) {
    EXPECT_THROW(AsyncSink(capture(), 0), std::invalid_argument);
    EXPECT_THROW(AsyncSink(nullptr, 4), std::invalid_argument);
}

TEST(Part4Async, ReportsCapacity) {
    AsyncSink async(capture(), 7);
    EXPECT_EQ(async.capacity(), 7u);
}

TEST(Part4Concurrency, ConcurrentProducersLoseNothingWithinCapacity) {
    auto target = capture();
    AsyncSink async(target, 2000);
    std::vector<std::thread> producers;
    for (int t = 0; t < 4; ++t) {
        producers.emplace_back([&async, t] {
            for (int i = 0; i < 250; ++i) {
                async.write(make_record(Level::info, "app", std::format("t{}:m{}", t, i)));
            }
        });
    }
    for (auto& p : producers) p.join();

    async.flush();

    EXPECT_EQ(async.dropped(), 0u);
    EXPECT_EQ(target->records().size(), 1000u);
}

TEST(Part4Concurrency, ConcurrentOstreamWritesDoNotInterleaveLines) {
    std::ostringstream out;
    // Formatter appends '|' so every line ends with exactly one delimiter.
    OstreamSink sink(out, [](const Record& r) { return r.message + "|"; });
    std::vector<std::thread> writers;
    for (int t = 0; t < 8; ++t) {
        writers.emplace_back([&sink, t] {
            for (int i = 0; i < 500; ++i) {
                sink.write(make_record(Level::info, "app", std::format("t{}:m{:03}", t, i)));
            }
        });
    }
    for (auto& w : writers) w.join();

    auto lines = split_lines(out.str());
    ASSERT_EQ(lines.size(), 4000u);
    for (const auto& line : lines) {
        ASSERT_EQ(line.size(), 8u) << line;  // "t<d>:m<ddd>|"
        EXPECT_EQ(line.front(), 't') << line;
        EXPECT_EQ(line[2], ':') << line;
        EXPECT_EQ(line.back(), '|') << line;
    }
}

TEST(Part4Concurrency, ConcurrentGetReturnsOneLoggerPerName) {
    Registry reg;
    constexpr int kThreads = 8;
    constexpr int kNames = 200;
    std::vector<std::vector<const Logger*>> seen(kThreads, std::vector<const Logger*>(kNames));
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&reg, &seen, t] {
            for (int k = 0; k < kNames; ++k) {
                seen[t][k] = &reg.get(std::format("app.worker.{}", k));
            }
        });
    }
    for (auto& th : threads) th.join();

    for (int k = 0; k < kNames; ++k) {
        for (int t = 1; t < kThreads; ++t) {
            EXPECT_EQ(seen[t][k], seen[0][k]) << "name " << k;
        }
    }
    EXPECT_NE(seen[0][0], seen[0][1]);
}

// ---------------------------------------------------------------------------
// Part 5 (async): MDC is captured on the logging thread, not at delivery time
// ---------------------------------------------------------------------------

TEST(Part5Mdc, FieldsAreCapturedAtLogTimeNotDeliveryTime) {
    auto target = capture();
    auto async = std::make_shared<AsyncSink>(target, 16);
    Registry reg;
    Logger& log = reg.get("svc");
    log.add_sink(async);
    {
        MdcScope scope("request_id", "r-42");
        CPPREF_INFO(log, "accepted");
    }
    // The scope is gone before the worker delivers the record.
    async->flush();

    auto records = target->records();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].fields, (std::vector<Field>{{"request_id", "r-42"}}));
}
