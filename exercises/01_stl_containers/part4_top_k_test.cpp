#include "top_k.hpp"

#include <cstdint>
#include <ranges>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

using cppref::stl::SalesLine;
using cppref::stl::SkuTotal;
using cppref::stl::top_k_products;

}  // namespace

TEST(Part4TopK, ReturnsTopKByUnitsDescending) {
    const std::vector<SalesLine> lines = {
        {"A", 5}, {"B", 9}, {"C", 2}, {"A", 7}, {"D", 9},  // totals: A=12 B=9 C=2 D=9
    };

    EXPECT_EQ(top_k_products(lines, 2), (std::vector<SkuTotal>{{"A", 12}, {"B", 9}}));
}

TEST(Part4TopK, TiesAreOrderedBySkuAscending) {
    const std::vector<SalesLine> lines = {{"Z", 5}, {"M", 5}, {"A", 5}};

    EXPECT_EQ(top_k_products(lines, 3),
              (std::vector<SkuTotal>{{"A", 5}, {"M", 5}, {"Z", 5}}));
}

TEST(Part4TopK, AggregatesRepeatedSkusAcrossALongStream) {
    std::vector<SalesLine> lines;
    lines.reserve(10000);
    for (int i = 0; i < 10000; ++i) {
        lines.push_back({"sku-" + std::to_string(i % 20), 1});
    }

    const auto top = top_k_products(lines, 100);

    ASSERT_EQ(top.size(), 20u);  // k larger than the distinct count returns all of them
    for (const auto& total : top) {
        EXPECT_EQ(total.units, 500);
    }
    EXPECT_EQ(top.front().sku, "sku-0");   // equal units: ascending SKU
    EXPECT_EQ(top.back().sku, "sku-9");    // lexicographically last
}

TEST(Part4TopK, KZeroIsEmptyAndEmptyStreamIsEmpty) {
    const std::vector<SalesLine> lines = {{"A", 1}};
    EXPECT_TRUE(top_k_products(lines, 0).empty());

    const std::vector<SalesLine> none;
    EXPECT_TRUE(top_k_products(none, 3).empty());
}

TEST(Part4TopK, NegativeUnitsAreRejected) {
    const std::vector<SalesLine> lines = {{"A", 4}, {"B", -1}};

    EXPECT_THROW(top_k_products(lines, 3), std::invalid_argument);
}

TEST(Part4TopK, ConsumesSingleInputPassExactlyOnce) {
    int produced = 0;
    auto lines = std::views::iota(0, 500) | std::views::transform([&produced](int i) {
                     ++produced;
                     return SalesLine{"sku-" + std::to_string(i % 7), i % 5};
                 });

    const auto top = top_k_products(lines, 3);

    EXPECT_EQ(produced, 500);  // a second traversal would double this
    ASSERT_EQ(top.size(), 3u);
    std::int64_t total_units = 0;
    for (const auto& t : top) total_units += t.units;
    EXPECT_GT(total_units, 0);
}
