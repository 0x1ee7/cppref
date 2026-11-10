#include "versioned_api.hpp"

#include <gtest/gtest.h>

#include <string>
#include <type_traits>
#include <vector>

using namespace orders;

// --- Part1: inline namespaces, ADL and a JSON encoder -----------------------

TEST(Part1VersionedApi, UnqualifiedOrderIsV2) {
    static_assert(std::is_same_v<orders::Order, orders::v2::Order>);
    orders::v2::Order o{"ord-1", 3, "EUR"};
    EXPECT_EQ(describe(o), "ord-1 x3 EUR");
}

TEST(Part1VersionedApi, V2DescribeIncludesCurrency) {
    orders::v2::Order o{"ord-1", 3, "EUR"};
    EXPECT_EQ(orders::v2::describe(o), "ord-1 x3 EUR");
}

TEST(Part1VersionedApi, V1IsDistinctTypeAndDescribes) {
    orders::v1::Order o{"ord-2", 5};
    EXPECT_FALSE((std::is_same_v<orders::v1::Order, orders::v2::Order>));
    EXPECT_EQ(orders::v1::describe(o), "ord-2 x5");
}

TEST(Part1VersionedApi, ToJsonFoundByAdlForV2) {
    orders::v2::Order o{"ord-1", 3, "EUR"};
    EXPECT_EQ(to_json(o), R"({"id":"ord-1","quantity":3,"currency":"EUR"})");
}

TEST(Part1VersionedApi, ToJsonFoundByAdlForV1) {
    orders::v1::Order o{"ord-2", 5};
    EXPECT_EQ(to_json(o), R"({"id":"ord-2","quantity":5})");
}

TEST(Part1VersionedApi, ToJsonEscapesQuotesInStrings) {
    orders::v1::Order o{"a\"b", 1};
    EXPECT_EQ(to_json(o), "{\"id\":\"a\\\"b\",\"quantity\":1}");
}

TEST(Part1VersionedApi, QuoteEscapesQuotesBackslashesAndNewlines) {
    EXPECT_EQ(json::quote("a\"b\\c\n"), "\"a\\\"b\\\\c\\n\"");
}

TEST(Part1VersionedApi, QuoteUsesUnicodeEscapeForControlChars) {
    EXPECT_EQ(json::quote(std::string("\x01", 1)), "\"\\u0001\"");
}

TEST(Part1VersionedApi, DumpJoinsEncodedValuesAsArray) {
    std::vector<std::string> items{json::quote("a"), "2"};
    EXPECT_EQ(json::dump(items), R"(["a",2])");
    EXPECT_EQ(json::dump({}), "[]");
}
