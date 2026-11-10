// Part 7: lazy k-way merge of time-ordered log streams.
#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "generator.hpp"
#include "log_records.hpp"
#include "merge.hpp"

namespace ir = cppref::iterators_and_ranges;
using ir::Generator;
using ir::Level;
using ir::LogRecord;

namespace {

std::chrono::sys_seconds at(std::int64_t s) {
    return std::chrono::sys_seconds{std::chrono::seconds{s}};
}

LogRecord rec(std::int64_t t, std::string server) {
    return LogRecord{at(t), Level::info, std::move(server), "m"};
}

Generator<LogRecord> stream_of(std::vector<LogRecord> records) {
    for (const auto& r : records) {
        co_yield r;
    }
}

// Infinite stream; `pulled` counts how many records the body has produced.
Generator<LogRecord> ticks(std::int64_t start, std::int64_t step, std::string server,
                           int* pulled = nullptr) {
    for (std::int64_t t = start;; t += step) {
        if (pulled != nullptr) {
            ++*pulled;
        }
        co_yield rec(t, server);
    }
}

Generator<LogRecord> then_fails() {
    co_yield rec(1, "bad");
    throw std::runtime_error("source failed");
}

template <typename G>
std::vector<LogRecord> drain(G&& gen) {
    std::vector<LogRecord> out;
    for (LogRecord r : gen) {
        out.push_back(std::move(r));
    }
    return out;
}

std::vector<std::int64_t> stamps(const std::vector<LogRecord>& records) {
    std::vector<std::int64_t> out;
    for (const auto& r : records) {
        out.push_back(r.ts.time_since_epoch().count());
    }
    return out;
}

std::vector<std::string> servers(const std::vector<LogRecord>& records) {
    std::vector<std::string> out;
    for (const auto& r : records) {
        out.push_back(r.server);
    }
    return out;
}

}  // namespace

TEST(Part7Merge, MergesTwoStreamsInTimestampOrder) {
    std::vector<Generator<LogRecord>> streams;
    streams.push_back(stream_of({rec(1, "a"), rec(4, "a"), rec(6, "a")}));
    streams.push_back(stream_of({rec(2, "b"), rec(3, "b"), rec(7, "b")}));
    EXPECT_EQ(stamps(drain(ir::merge_sorted(std::move(streams)))),
              (std::vector<std::int64_t>{1, 2, 3, 4, 6, 7}));
}

TEST(Part7Merge, ManyStreamsInterleaveCorrectly) {
    std::vector<Generator<LogRecord>> streams;
    streams.push_back(stream_of({rec(1, "a"), rec(4, "a"), rec(7, "a")}));
    streams.push_back(stream_of({rec(2, "b"), rec(5, "b"), rec(8, "b")}));
    streams.push_back(stream_of({rec(3, "c"), rec(6, "c"), rec(9, "c")}));
    EXPECT_EQ(stamps(drain(ir::merge_sorted(std::move(streams)))),
              (std::vector<std::int64_t>{1, 2, 3, 4, 5, 6, 7, 8, 9}));
}

TEST(Part7Merge, TiesGoToLowerStreamIndex) {
    std::vector<Generator<LogRecord>> streams;
    streams.push_back(stream_of({rec(5, "a1"), rec(5, "a2")}));
    streams.push_back(stream_of({rec(5, "b1")}));
    EXPECT_EQ(servers(drain(ir::merge_sorted(std::move(streams)))),
              (std::vector<std::string>{"a1", "a2", "b1"}));
}

TEST(Part7Merge, EmptyInputsContributeNothing) {
    EXPECT_TRUE(drain(ir::merge_sorted(std::vector<Generator<LogRecord>>{})).empty());

    std::vector<Generator<LogRecord>> streams;
    streams.push_back(stream_of({}));
    streams.push_back(stream_of({rec(1, "x"), rec(2, "x")}));
    streams.push_back(stream_of({}));
    EXPECT_EQ(stamps(drain(ir::merge_sorted(std::move(streams)))),
              (std::vector<std::int64_t>{1, 2}));
}

TEST(Part7Merge, OutputKeepsRecordContents) {
    std::vector<Generator<LogRecord>> streams;
    streams.push_back(stream_of({rec(1, "a")}));
    auto out = drain(ir::merge_sorted(std::move(streams)));
    ASSERT_EQ(out.size(), 1U);
    EXPECT_EQ(out[0], rec(1, "a"));
}

TEST(Part7Merge, WorksWithInfiniteStreams) {
    std::vector<Generator<LogRecord>> streams;
    streams.push_back(ticks(0, 2, "even"));
    streams.push_back(ticks(1, 2, "odd"));
    auto merged = ir::merge_sorted(std::move(streams));
    std::vector<LogRecord> out;
    auto it = merged.begin();
    for (int i = 0; i < 5; ++i) {
        out.push_back(*it);
        ++it;
    }
    EXPECT_EQ(stamps(out), (std::vector<std::int64_t>{0, 1, 2, 3, 4}));
}

TEST(Part7Merge, PullsInputsOnlyAsFarAsOutputRequires) {
    int pulled_a = 0;
    int pulled_b = 0;
    std::vector<Generator<LogRecord>> streams;
    streams.push_back(ticks(1, 1, "a", &pulled_a));
    streams.push_back(ticks(100, 1, "b", &pulled_b));
    auto merged = ir::merge_sorted(std::move(streams));
    auto it = merged.begin();
    EXPECT_EQ((*it).ts, at(1));
    ++it;
    EXPECT_EQ((*it).ts, at(2));
    // Stream b is never needed before t=100; stream a only needs its next head.
    EXPECT_GE(pulled_a, 2);
    EXPECT_LE(pulled_a, 3);
    EXPECT_LE(pulled_b, 1);
}

TEST(Part7Merge, PropagatesStreamExceptions) {
    std::vector<Generator<LogRecord>> streams;
    streams.push_back(then_fails());
    streams.push_back(stream_of({rec(2, "ok")}));
    EXPECT_THROW((void)drain(ir::merge_sorted(std::move(streams))), std::runtime_error);
}
