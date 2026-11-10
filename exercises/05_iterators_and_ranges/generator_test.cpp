// Part 4: a minimal coroutine-backed Generator<T>.
#include <gtest/gtest.h>

#include <cstddef>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "generator.hpp"

namespace ir = cppref::iterators_and_ranges;
namespace views = std::ranges::views;

namespace {

ir::Generator<int> counting(int n, int* started) {
    if (started != nullptr) {
        ++*started;
    }
    for (int i = 0; i < n; ++i) {
        co_yield i;
    }
}

struct Probe {
    int* destroyed;
    ~Probe() { ++*destroyed; }
};

ir::Generator<int> with_probe(int* destroyed, int n) {
    Probe probe{destroyed};
    for (int i = 0; i < n; ++i) {
        co_yield i;
    }
}

ir::Generator<int> naturals() {
    for (int i = 0;; ++i) {
        co_yield i;
    }
}

ir::Generator<int> fails_after_one() {
    co_yield 1;
    throw std::runtime_error("boom");
}

ir::Generator<int> fails_immediately(bool fail) {
    if (fail) {
        throw std::runtime_error("early");
    }
    co_yield 0;
}

ir::Generator<std::string> words() {
    co_yield std::string("alpha");
    co_yield std::string("beta");
}

}  // namespace

TEST(Part4Generator, BodyDoesNotRunUntilBegin) {
    int started = 0;
    auto gen = counting(3, &started);
    EXPECT_EQ(started, 0);
    auto it = gen.begin();
    EXPECT_EQ(started, 1);
    EXPECT_EQ(*it, 0);
}

TEST(Part4Generator, YieldsValuesInOrderIncludingEmptyBody) {
    std::vector<int> out;
    for (int v : counting(4, nullptr)) {
        out.push_back(v);
    }
    EXPECT_EQ(out, (std::vector<int>{0, 1, 2, 3}));

    auto empty = counting(0, nullptr);
    EXPECT_TRUE(empty.begin() == empty.end());
}

TEST(Part4Generator, ComposesWithViewsOnInfiniteBody) {
    auto nat = naturals();
    auto result = nat | views::filter([](int x) { return x % 2 == 1; }) | views::take(3) |
                  std::ranges::to<std::vector<int>>();
    EXPECT_EQ(result, (std::vector<int>{1, 3, 5}));
}

TEST(Part4Generator, ExceptionIsRethrownOnResume) {
    auto gen = fails_after_one();
    auto it = gen.begin();
    EXPECT_EQ(*it, 1);
    EXPECT_THROW(++it, std::runtime_error);
}

TEST(Part4Generator, ExceptionIsRethrownFromBegin) {
    auto gen = fails_immediately(true);
    EXPECT_THROW((void)gen.begin(), std::runtime_error);
}

TEST(Part4Generator, FinishedAfterException) {
    auto gen = fails_after_one();
    auto it = gen.begin();
    EXPECT_THROW(++it, std::runtime_error);
    EXPECT_TRUE(it == gen.end());
}

TEST(Part4Generator, EarlyBreakReleasesFrameLocals) {
    int destroyed = 0;
    {
        auto gen = with_probe(&destroyed, 10);
        for (int v : gen) {
            if (v == 2) {
                break;
            }
        }
        EXPECT_EQ(destroyed, 0);  // still alive while the generator is alive
    }
    EXPECT_EQ(destroyed, 1);
}

TEST(Part4Generator, CompletionReleasesFrameLocals) {
    int destroyed = 0;
    auto gen = with_probe(&destroyed, 3);
    int sum = 0;
    for (int v : gen) {
        sum += v;
    }
    EXPECT_EQ(sum, 3);
    EXPECT_EQ(destroyed, 1);
}

TEST(Part4Generator, DestroyingUnstartedGeneratorRunsNoBody) {
    int destroyed = 0;
    {
        auto gen = with_probe(&destroyed, 3);
        (void)gen;
    }
    EXPECT_EQ(destroyed, 0);
}

TEST(Part4Generator, IsMoveOnlyAndYieldsStringValues) {
    static_assert(!std::is_copy_constructible_v<ir::Generator<int>>);
    static_assert(std::is_move_constructible_v<ir::Generator<int>>);
    auto first = words();
    auto second = std::move(first);
    std::vector<std::string> out;
    for (std::string& w : second) {
        out.push_back(w);
    }
    EXPECT_EQ(out, (std::vector<std::string>{"alpha", "beta"}));
}
