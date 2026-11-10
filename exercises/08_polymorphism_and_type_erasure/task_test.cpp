// Part 5: type erasure - a move-only Task and a heterogeneous PropertyBag on std::any.
#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "task.hpp"

namespace poly = cppref::polymorphism;

namespace {

struct Job {
    int* hits;
    void run() { ++*hits; }
};

struct Both {
    int* run_hits;
    int* call_hits;
    void run() { ++*run_hits; }
    void operator()() { ++*call_hits; }
};

}  // namespace

// ---- Part 5a: Task ---------------------------------------------------------

TEST(Part5Task, CallableRunsOnRun) {
    int calls = 0;
    poly::Task t([&calls] { ++calls; });
    EXPECT_FALSE(t.empty());
    t.run();
    EXPECT_EQ(calls, 1);
}

TEST(Part5Task, MoveOnlyCallableKeepsStateAcrossRuns) {
    int out = 0;
    poly::Task t([p = std::make_unique<int>(41), &out]() mutable { out = ++*p; });
    t.run();
    EXPECT_EQ(out, 42);
    t.run();
    EXPECT_EQ(out, 43);
}

TEST(Part5Task, RunnableStructIsInvokedThroughRunMember) {
    int hits = 0;
    poly::Task t(Job{&hits});
    t.run();
    EXPECT_EQ(hits, 1);
}

TEST(Part5Task, RunMemberWinsOverCallOperator) {
    int run_hits = 0;
    int call_hits = 0;
    poly::Task t(Both{&run_hits, &call_hits});
    t.run();
    EXPECT_EQ(run_hits, 1);
    EXPECT_EQ(call_hits, 0);
}

TEST(Part5Task, LvalueCallableIsCopiedIn) {
    int hits = 0;
    auto inc = [&hits] { ++hits; };
    poly::Task t(inc);
    t.run();
    inc();
    EXPECT_EQ(hits, 2);
}

TEST(Part5Task, MoveTransfersOwnershipAndLeavesSourceEmpty) {
    int hits = 0;
    poly::Task a([&hits] { ++hits; });
    poly::Task b = std::move(a);
    EXPECT_TRUE(a.empty());
    EXPECT_FALSE(b.empty());
    b.run();
    EXPECT_EQ(hits, 1);
    EXPECT_THROW(a.run(), std::logic_error);
}

TEST(Part5Task, MoveAssignReplacesTarget) {
    int x = 0;
    int y = 0;
    poly::Task a([&x] { ++x; });
    poly::Task b([&y] { ++y; });
    b = std::move(a);
    b.run();
    EXPECT_EQ(x, 1);
    EXPECT_EQ(y, 0);
    EXPECT_TRUE(a.empty());
}

TEST(Part5Task, ExceptionPropagatesAndTaskStaysUsable) {
    int calls = 0;
    poly::Task t([&calls] {
        if (++calls == 1) {
            throw std::runtime_error("boom");
        }
    });
    EXPECT_THROW(t.run(), std::runtime_error);
    EXPECT_NO_THROW(t.run());
    EXPECT_EQ(calls, 2);
}

TEST(Part5Task, CopyIsDeletedAndRunnableConceptHoldsForLambdasAndMembers) {
    static_assert(!std::is_copy_constructible_v<poly::Task>);
    static_assert(poly::Runnable<Job>);
    static_assert(poly::Runnable<decltype([] {})>);
    static_assert(!poly::Runnable<int>);
    poly::Task t([] {});
    t.run();
    EXPECT_FALSE(t.empty());
}

// ---- Part 5b: PropertyBag --------------------------------------------------

TEST(Part5PropertyBag, SetAndGetTypedValues) {
    poly::PropertyBag bag;
    bag.set("port", 8080);
    bag.set("ids", std::vector<int>{1, 2, 3});

    const int* port = bag.get<int>("port");
    ASSERT_NE(port, nullptr);
    EXPECT_EQ(*port, 8080);

    const auto* ids = bag.get<std::vector<int>>("ids");
    ASSERT_NE(ids, nullptr);
    EXPECT_EQ(*ids, (std::vector<int>{1, 2, 3}));
    EXPECT_EQ(bag.size(), 2U);
}

TEST(Part5PropertyBag, TypeMismatchReturnsNullptr) {
    poly::PropertyBag bag;
    bag.set("port", 8080);
    EXPECT_EQ(bag.get<std::string>("port"), nullptr);
    EXPECT_EQ(bag.get<long>("port"), nullptr);
}

TEST(Part5PropertyBag, MissingKeyReturnsNullptrAndContainsIsFalse) {
    poly::PropertyBag bag;
    EXPECT_EQ(bag.get<int>("nope"), nullptr);
    EXPECT_FALSE(bag.contains("nope"));
    EXPECT_EQ(bag.size(), 0U);
}

TEST(Part5PropertyBag, SetReplacesValueAndType) {
    poly::PropertyBag bag;
    bag.set("k", 1);
    bag.set("k", std::string("two"));
    const auto* s = bag.get<std::string>("k");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(*s, "two");
    EXPECT_EQ(bag.get<int>("k"), nullptr);
    EXPECT_EQ(bag.size(), 1U);
}

TEST(Part5PropertyBag, EraseReportsWhetherAnEntryWasRemoved) {
    poly::PropertyBag bag;
    bag.set("a", 1);
    bag.set("b", std::string("x"));
    EXPECT_TRUE(bag.erase("a"));
    EXPECT_FALSE(bag.erase("a"));
    EXPECT_FALSE(bag.contains("a"));
    EXPECT_TRUE(bag.contains("b"));
    EXPECT_EQ(bag.size(), 1U);
}
