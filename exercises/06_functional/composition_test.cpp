#include "composition.hpp"

#include <gtest/gtest.h>

#include <format>
#include <string>

namespace fn = cppref::functional;

namespace {

auto inc = [](int x) { return x + 1; };
auto dbl = [](int x) { return x * 2; };
auto sq = [](int x) { return x * x; };
auto sub = [](int a, int b) { return a - b; };
auto to_text = [](int x) { return std::string("n=") + std::to_string(x); };

}  // namespace

TEST(Part1Composition, ComposeAppliesRightToLeft) {
    EXPECT_EQ(fn::compose(inc, dbl)(5), 11);  // dbl first: 10, then inc: 11
    EXPECT_EQ(fn::compose(dbl, inc)(5), 12);  // inc first: 6, then dbl: 12
}

TEST(Part1Composition, ComposeNestsThreeFunctions) {
    EXPECT_EQ(fn::compose(inc, dbl, sq)(3), 19);  // sq 9, dbl 18, inc 19
}

TEST(Part1Composition, ComposeWithNoFunctionsIsIdentity) {
    EXPECT_EQ(fn::compose()(42), 42);
}

TEST(Part1Composition, ComposeOwnsItsCallables) {
    // The capture dies when make() returns; the composed function must not
    // hold a dangling reference to it.
    auto make = [] {
        int k = 10;
        return fn::compose([k](int x) { return x + k; }, dbl);
    };
    auto f = make();
    EXPECT_EQ(f(1), 12);  // dbl(1) = 2, then + 10
}

TEST(Part1Composition, ComposeAllowsTypeChangingSteps) {
    EXPECT_EQ(std::format("{}", fn::compose(to_text, inc)(4)), "n=5");
}

TEST(Part1Composition, PipeThreadsValueLeftToRight) {
    EXPECT_EQ(fn::pipe(3, inc, dbl), 8);  // (3 + 1) * 2
    EXPECT_EQ(fn::pipe(3, dbl, inc), 7);  // (3 * 2) + 1
}

TEST(Part1Composition, PipeCanChangeType) {
    EXPECT_EQ(std::format("{}", fn::pipe(3, to_text)), "n=3");
    EXPECT_EQ(std::format("{}", fn::pipe(3, to_text, [](const std::string& s) { return s.size(); })),
              "3");
}

TEST(Part1Composition, PartialRightAppendsBoundArgument) {
    EXPECT_EQ(fn::partial_right(sub, 1)(10), 9);  // sub(10, 1)
}

TEST(Part1Composition, PartialRightBindsSeveralArguments) {
    auto f3 = [](int a, int b, int c) { return a * 100 + b * 10 + c; };
    EXPECT_EQ(fn::partial_right(f3, 2, 3)(1), 123);  // f3(1, 2, 3)
}
