#include "captures.hpp"

#include <gtest/gtest.h>

#include <functional>
#include <string>
#include <vector>

using namespace cppref::captures;

// --- Part5: lambda captures and lifetimes ---

TEST(Part5Captures, IdFactoriesHaveIndependentState) {
    auto a = make_id_factory(10);
    auto b = make_id_factory(10);
    EXPECT_EQ(a(), 10);
    EXPECT_EQ(a(), 11);
    EXPECT_EQ(b(), 10);
}

TEST(Part5Captures, IncrementerWritesThroughReference) {
    int n = 0;
    auto inc = make_incrementer(n);
    inc();
    inc();
    EXPECT_EQ(n, 2);
}

TEST(Part5Captures, StepCounterSequence) {
    auto c = make_step_counter(0, 3);
    EXPECT_EQ(c(), 0);
    EXPECT_EQ(c(), 3);
    EXPECT_EQ(c(), 6);
}

TEST(Part5Captures, StepCounterCopiesAdvanceIndependently) {
    auto c = make_step_counter(0, 5);
    EXPECT_EQ(c(), 0);
    auto d = c;  // copy carries the closure's current state (next value 5)
    EXPECT_EQ(d(), 5);
    EXPECT_EQ(c(), 5);
}

TEST(Part5Captures, AllowListCapturesByValue) {
    std::vector<std::string> names{"a"};
    auto allowed = make_allow_list(names);
    names.clear();
    EXPECT_TRUE(allowed("a"));
    EXPECT_FALSE(allowed("b"));
}

TEST(Part5Captures, SchedulerAdderSurvivesSchedulerDestruction) {
    std::function<int(int)> add;
    {
        Scheduler s(100);
        add = s.make_adder();
    }
    EXPECT_EQ(add(5), 105);
}

TEST(Part5Captures, SchedulerAdderAddsBase) {
    Scheduler s(7);
    EXPECT_EQ(s.make_adder()(3), 10);
}
