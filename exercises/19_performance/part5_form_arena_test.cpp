// Part 5 tests: request-scoped parsing into a monotonic arena.
#include "form_arena.hpp"
#include "perf_support_test.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <memory_resource>
#include <new>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using cppref::performance::FormField;
using cppref::performance::FormParseErrorKind;
using cppref::performance::parse_form;
using cppref::performance::testing::AllocScope;

// Key/value pairs copied out of the arena for comparison.
std::vector<std::pair<std::string, std::string>> pairs_of(
    const std::pmr::vector<FormField>& fields) {
    std::vector<std::pair<std::string, std::string>> out;
    for (const auto& f : fields) {
        out.emplace_back(std::string(std::string_view(f.key)), std::string(std::string_view(f.value)));
    }
    return out;
}

// Arena over a stack buffer; exhaustion throws std::bad_alloc (null upstream).
class FormFixture : public ::testing::Test {
protected:
    std::pmr::memory_resource* arena() { return &arena_; }

    std::array<std::byte, 8192> buffer_{};
    std::pmr::monotonic_buffer_resource arena_{buffer_.data(), buffer_.size(),
                                               std::pmr::null_memory_resource()};
};

class TinyFormFixture : public ::testing::Test {
protected:
    std::pmr::memory_resource* arena() { return &arena_; }

    std::array<std::byte, 64> buffer_{};
    std::pmr::monotonic_buffer_resource arena_{buffer_.data(), buffer_.size(),
                                               std::pmr::null_memory_resource()};
};

TEST_F(FormFixture, ParsesKeyValuePairs) {
    auto r = parse_form("name=alice&role=admin", arena());
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(pairs_of(*r), (std::vector<std::pair<std::string, std::string>>{
                                {"name", "alice"}, {"role", "admin"}}));
}

TEST_F(FormFixture, SkipsEmptySegments) {
    auto r = parse_form("a=1&&b=2&", arena());
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(pairs_of(*r), (std::vector<std::pair<std::string, std::string>>{
                                {"a", "1"}, {"b", "2"}}));
}

TEST_F(FormFixture, FirstEqualsSplitsKeyFromValue) {
    auto r = parse_form("k=a=b", arena());
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(pairs_of(*r), (std::vector<std::pair<std::string, std::string>>{{"k", "a=b"}}));
}

TEST_F(FormFixture, SegmentWithoutEqualsHasEmptyValue) {
    auto r = parse_form("flag", arena());
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(pairs_of(*r), (std::vector<std::pair<std::string, std::string>>{{"flag", ""}}));
}

TEST_F(FormFixture, PlusDecodesToSpace) {
    auto r = parse_form("q=hello+world", arena());
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(pairs_of(*r), (std::vector<std::pair<std::string, std::string>>{
                                {"q", "hello world"}}));
}

TEST_F(FormFixture, PercentEscapesDecodeInEitherHexCase) {
    auto r = parse_form("x=%41%2f%2F", arena());
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(pairs_of(*r), (std::vector<std::pair<std::string, std::string>>{{"x", "A//"}}));
}

TEST_F(FormFixture, EmptyBodyYieldsNoFields) {
    auto r = parse_form("", arena());
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->empty());
}

TEST_F(FormFixture, ResultKeepsArenaAfterMove) {
    auto r = parse_form("a=1", arena());
    ASSERT_TRUE(r.has_value());
    std::pmr::vector<FormField> moved(std::move(*r));
    EXPECT_EQ(moved.size(), 1u);
    EXPECT_EQ(moved.get_allocator().resource(), arena());
}

TEST_F(FormFixture, BadEscapeReportsPercentOffset) {
    auto r = parse_form("x=1&y=%zz", arena());
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().kind, FormParseErrorKind::bad_escape);
    EXPECT_EQ(r.error().offset, 6u);
}

TEST_F(FormFixture, TruncatedEscapeAtEndIsBadEscape) {
    auto r = parse_form("a=%4", arena());
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().kind, FormParseErrorKind::bad_escape);
    EXPECT_EQ(r.error().offset, 2u);
}

TEST_F(FormFixture, EmptyKeyReportsSegmentStart) {
    auto r = parse_form("a=1&=2", arena());
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().kind, FormParseErrorKind::empty_key);
    EXPECT_EQ(r.error().offset, 4u);
}

TEST_F(FormFixture, LoneEqualsIsEmptyKey) {
    auto r = parse_form("=", arena());
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().kind, FormParseErrorKind::empty_key);
    EXPECT_EQ(r.error().offset, 0u);
}

TEST_F(FormFixture, FieldsComeFromArenaNotGlobalHeap) {
    // Keys longer than the small-string buffer, so every string would allocate if it used the heap.
    std::string body;
    for (int i = 0; i < 30; ++i) {
        if (i > 0) body += '&';
        body += "a_rather_long_key_" + std::to_string(i) + "=value_" + std::to_string(i);
    }
    AllocScope scope;
    auto r = parse_form(body, arena());
    const std::size_t allocs = scope.allocations();
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->size(), 30u);
    EXPECT_EQ(allocs, 0u);
}

TEST_F(TinyFormFixture, ExhaustedArenaThrowsBadAlloc) {
    const std::string body = "a=1&b=2&c=3&d=4&e=5&f=6&g=7&h=8&i=9&j=10";
    EXPECT_THROW(parse_form(body, arena()), std::bad_alloc);
}

}  // namespace
