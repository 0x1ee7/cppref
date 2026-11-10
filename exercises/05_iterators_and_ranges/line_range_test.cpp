// Part 1: LineRange, a single-pass input view over an std::istream.
#include <gtest/gtest.h>

#include <cstddef>
#include <iterator>
#include <ranges>
#include <sstream>
#include <string>
#include <vector>

#include "line_range.hpp"

namespace ir = cppref::iterators_and_ranges;
namespace views = std::ranges::views;

namespace {

std::vector<std::string> read_all(std::istream& in) {
    std::vector<std::string> out;
    for (const auto& line : ir::LineRange(in)) {
        out.push_back(line);
    }
    return out;
}

}  // namespace

TEST(Part1LineRange, YieldsLinesWithoutNewlines) {
    std::istringstream in("alpha\nbeta\ngamma\n");
    EXPECT_EQ(read_all(in), (std::vector<std::string>{"alpha", "beta", "gamma"}));
}

TEST(Part1LineRange, LastLineWithoutTrailingNewlineIsKept) {
    std::istringstream in("one\ntwo");
    EXPECT_EQ(read_all(in), (std::vector<std::string>{"one", "two"}));
}

TEST(Part1LineRange, EmptyStreamIsEmptyRange) {
    std::istringstream in("");
    ir::LineRange range(in);
    auto first = range.begin();
    EXPECT_TRUE(first == range.end());
}

TEST(Part1LineRange, BlankLinesArePreserved) {
    std::istringstream in("a\n\nb\n");
    EXPECT_EQ(read_all(in), (std::vector<std::string>{"a", "", "b"}));
}

TEST(Part1LineRange, ReadsOnlyAsFarAsConsumed) {
    std::istringstream in("a\nb\nc\n");
    ir::LineRange range(in);
    auto it = range.begin();
    EXPECT_EQ(*it, "a");
    // Nothing past the first line may have been read from the stream.
    std::string rest((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(rest, "b\nc\n");
}

TEST(Part1LineRange, ComposesWithStandardAdaptorsAsSinglePassInput) {
    static_assert(std::ranges::input_range<ir::LineRange>);
    static_assert(!std::ranges::forward_range<ir::LineRange>);

    std::istringstream in("1\n\n22\n\n333\n");
    std::vector<std::size_t> lengths;
    for (std::size_t n : ir::LineRange(in) |
                             views::filter([](const std::string& s) { return !s.empty(); }) |
                             views::transform([](const std::string& s) { return s.size(); })) {
        lengths.push_back(n);
    }
    EXPECT_EQ(lengths, (std::vector<std::size_t>{1, 2, 3}));
}
