// Part 3: lazily paginated input range over an injected fetch function.
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "paged_range.hpp"

namespace ir = cppref::iterators_and_ranges;
namespace views = std::ranges::views;
using Cursor = std::optional<std::string>;
using Mock = testing::MockFunction<ir::Page<int>(const Cursor&)>;
using testing::_;
using testing::Invoke;
using testing::Return;
using testing::Throw;

namespace {

ir::Page<int> page(std::vector<int> items, Cursor next) {
    return ir::Page<int>{std::move(items), std::move(next)};
}

template <typename R>
std::vector<int> drain(R&& range) {
    std::vector<int> out;
    for (const int& v : range) {
        out.push_back(v);
    }
    return out;
}

}  // namespace

TEST(Part3Pagination, ConstructionDoesNotFetch) {
    Mock mock;
    EXPECT_CALL(mock, Call(_)).Times(0);
    auto range = ir::paginate(mock.AsStdFunction());
    (void)range;
}

TEST(Part3Pagination, FlattensPagesInOrder) {
    Mock mock;
    testing::InSequence seq;
    EXPECT_CALL(mock, Call(Cursor{})).WillOnce(Return(page({1, 2}, Cursor{"c1"})));
    EXPECT_CALL(mock, Call(Cursor{"c1"})).WillOnce(Return(page({3}, Cursor{})));
    EXPECT_EQ(drain(ir::paginate(mock.AsStdFunction())), (std::vector<int>{1, 2, 3}));
}

TEST(Part3Pagination, StartCursorReplacesTheFirstRequest) {
    Mock mock;
    EXPECT_CALL(mock, Call(Cursor{"resume"})).WillOnce(Return(page({7}, Cursor{})));
    EXPECT_EQ(drain(ir::paginate(mock.AsStdFunction(), Cursor{"resume"})),
              (std::vector<int>{7}));
}

TEST(Part3Pagination, EmptyPagesAreSkipped) {
    Mock mock;
    testing::InSequence seq;
    EXPECT_CALL(mock, Call(Cursor{})).WillOnce(Return(page({}, Cursor{"c1"})));
    EXPECT_CALL(mock, Call(Cursor{"c1"})).WillOnce(Return(page({4}, Cursor{})));
    EXPECT_EQ(drain(ir::paginate(mock.AsStdFunction())), (std::vector<int>{4}));
}

TEST(Part3Pagination, FetchesNextPageOnlyWhenNeeded) {
    Mock mock;
    bool second_fetched = false;
    EXPECT_CALL(mock, Call(Cursor{})).WillOnce(Return(page({1}, Cursor{"c1"})));
    EXPECT_CALL(mock, Call(Cursor{"c1"})).WillOnce(Invoke([&second_fetched](const Cursor&) {
        second_fetched = true;
        return page({2}, Cursor{});
    }));

    auto range = ir::paginate(mock.AsStdFunction());
    auto it = range.begin();
    EXPECT_EQ(*it, 1);
    EXPECT_FALSE(second_fetched);
    ++it;
    EXPECT_EQ(*it, 2);
    EXPECT_TRUE(second_fetched);
}

TEST(Part3Pagination, RepeatedCursorThrowsPaginationLoopError) {
    Mock mock;
    testing::InSequence seq;
    EXPECT_CALL(mock, Call(Cursor{})).WillOnce(Return(page({1}, Cursor{"c1"})));
    EXPECT_CALL(mock, Call(Cursor{"c1"})).WillOnce(Return(page({2}, Cursor{"c1"})));
    EXPECT_THROW((void)drain(ir::paginate(mock.AsStdFunction())), ir::PaginationLoopError);
}

TEST(Part3Pagination, StartCursorCountsAsRequested) {
    Mock mock;
    EXPECT_CALL(mock, Call(Cursor{"s"})).WillOnce(Return(page({1}, Cursor{"s"})));
    EXPECT_THROW((void)drain(ir::paginate(mock.AsStdFunction(), Cursor{"s"})),
                 ir::PaginationLoopError);
}

TEST(Part3Pagination, FetchExceptionsPropagateToConsumer) {
    Mock mock;
    testing::InSequence seq;
    EXPECT_CALL(mock, Call(Cursor{})).WillOnce(Return(page({1}, Cursor{"c1"})));
    EXPECT_CALL(mock, Call(Cursor{"c1"})).WillOnce(Throw(std::runtime_error("boom")));
    EXPECT_THROW((void)drain(ir::paginate(mock.AsStdFunction())), std::runtime_error);
}

TEST(Part3Pagination, ComposesWithStandardAdaptors) {
    Mock mock;
    testing::InSequence seq;
    EXPECT_CALL(mock, Call(Cursor{})).WillOnce(Return(page({1, 2}, Cursor{"c1"})));
    EXPECT_CALL(mock, Call(Cursor{"c1"})).WillOnce(Return(page({3, 4}, Cursor{})));
    auto doubled_evens = ir::paginate(mock.AsStdFunction()) |
                         views::filter([](int v) { return v % 2 == 0; }) |
                         views::transform([](int v) { return v * 10; });
    EXPECT_EQ(drain(std::move(doubled_evens)), (std::vector<int>{20, 40}));
}
