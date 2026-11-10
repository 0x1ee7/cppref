// Part 7: keyset/offset pagination with an opaque cursor; N+1 avoidance.

#include "model.hpp"
#include "order_queries.hpp"
#include "repositories.hpp"
#include "sqlite_wrapper.hpp"
#include "test_support_test.hpp"

#include <gtest/gtest.h>
#include <sqlite3.h>

#include <cctype>
#include <cstdint>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace db = cppref::database_sqlite;
using cppref::database_sqlite::testing::TestDb;
using cppref::database_sqlite::testing::at;
using cppref::database_sqlite::testing::seed_catalogue;

namespace {

// Places a one-line order for customer at the given epoch second; returns its id.
std::int64_t place_one(db::Database& database, std::int64_t customer, std::int64_t seconds,
                       std::int64_t quantity = 1) {
    db::OrderRepository orders(database);
    const std::vector<db::OrderLineInput> lines{{"WIDGET", quantity}};
    return orders.place(customer, at(seconds), lines);
}

std::int64_t add_customer(db::Database& database, const std::string& email) {
    return db::CustomerRepository(database).add(email, "Customer " + email, at(1)).id;
}

// Walks every page with the given size, collecting order ids in visit order.
std::vector<std::int64_t> walk_keyset(db::Database& database, std::size_t limit) {
    std::vector<std::int64_t> ids;
    std::optional<db::OrderCursor> after;
    for (int guard = 0; guard < 1000; ++guard) {
        const auto page = db::page_orders_keyset(database, after, limit);
        for (const auto& order : page.items) {
            ids.push_back(order.id);
        }
        after = page.next_cursor;
        if (!after) {
            break;
        }
    }
    return ids;
}

// Statement executions observed while fn runs (sqlite3_trace_v2, SQLITE_TRACE_STMT).
int g_statement_events = 0;
int count_statement(unsigned event, void*, void*, void*) {
    if (event == SQLITE_TRACE_STMT) {
        ++g_statement_events;
    }
    return 0;
}

template <class Fn>
int statements_during(db::Database& database, Fn fn) {
    g_statement_events = 0;
    sqlite3_trace_v2(database.handle(), SQLITE_TRACE_STMT, count_statement, nullptr);
    fn();
    sqlite3_trace_v2(database.handle(), 0, nullptr, nullptr);
    return g_statement_events;
}

}  // namespace

// ---- Part 7: cursor, keyset pages, offset pages, N+1 avoidance ------------------

TEST(Part7Cursor, EncodeDecodeRoundTripsAndIsUrlSafe) {
    const db::OrderCursor cursor{at(1700000000), 42};
    const std::string token = cursor.encode();
    EXPECT_EQ(db::OrderCursor::decode(token), cursor);
    for (char ch : token) {
        EXPECT_TRUE(std::isalnum(static_cast<unsigned char>(ch)) || ch == '-' || ch == '_')
            << "unexpected character in token: " << token;
    }
}

TEST(Part7Cursor, DecodeRejectsMalformedTokens) {
    for (const char* bad : {"", "!!!", "abc", "not-base64-at-all$", "AAAA"}) {
        EXPECT_THROW((void)db::OrderCursor::decode(bad), std::invalid_argument) << "token: " << bad;
    }
}

TEST(Part7Keyset, PagesCoverEveryRowExactlyOnceInOrder) {
    TestDb test;
    seed_catalogue(test.db());
    const auto customer = add_customer(test.db(), "a@x.test");
    for (int i = 0; i < 25; ++i) {
        place_one(test.db(), customer, 1000 + i);
    }

    std::vector<std::int64_t> expected(25);
    std::iota(expected.begin(), expected.end(), 1);
    EXPECT_EQ(walk_keyset(test.db(), 10), expected);
}

TEST(Part7Keyset, FinalShortPageHasNoCursor) {
    TestDb test;
    seed_catalogue(test.db());
    const auto customer = add_customer(test.db(), "a@x.test");
    for (int i = 0; i < 20; ++i) {
        place_one(test.db(), customer, 1000 + i);
    }
    const auto second = db::page_orders_keyset(test.db(), db::page_orders_keyset(test.db(), std::nullopt, 10).next_cursor, 10);
    EXPECT_EQ(second.items.size(), 10u);
    EXPECT_FALSE(second.next_cursor.has_value() && second.items.empty());
    const auto third = db::page_orders_keyset(test.db(), second.next_cursor, 10);
    EXPECT_TRUE(third.items.empty());
    EXPECT_FALSE(third.next_cursor.has_value());
}

TEST(Part7Keyset, TiesOnTimestampAreBrokenByIdWithoutDuplicates) {
    TestDb test;
    seed_catalogue(test.db());
    const auto customer = add_customer(test.db(), "a@x.test");
    for (int i = 0; i < 7; ++i) {
        place_one(test.db(), customer, 500);
    }
    std::vector<std::int64_t> expected(7);
    std::iota(expected.begin(), expected.end(), 1);
    EXPECT_EQ(walk_keyset(test.db(), 3), expected);
}

TEST(Part7Keyset, OlderInsertsDoNotDuplicateOrSkipRows) {
    TestDb test;
    seed_catalogue(test.db());
    const auto customer = add_customer(test.db(), "a@x.test");
    for (int i = 0; i < 5; ++i) {
        place_one(test.db(), customer, 1000 + i);
    }
    const auto first = db::page_orders_keyset(test.db(), std::nullopt, 2);
    ASSERT_EQ(first.items.size(), 2u);
    place_one(test.db(), customer, 500);  // sorts before the cursor

    const auto second = db::page_orders_keyset(test.db(), first.next_cursor, 2);
    ASSERT_EQ(second.items.size(), 2u);
    EXPECT_EQ(second.items[0].id, 3);
    EXPECT_EQ(second.items[1].id, 4);
}

TEST(Part7Keyset, ZeroLimitIsRejected) {
    TestDb test;
    EXPECT_THROW((void)db::page_orders_keyset(test.db(), std::nullopt, 0), std::invalid_argument);
}

TEST(Part7Offset, OffsetPagesWalkTheSameOrderAsKeyset) {
    TestDb test;
    seed_catalogue(test.db());
    const auto customer = add_customer(test.db(), "a@x.test");
    for (int i = 0; i < 12; ++i) {
        place_one(test.db(), customer, 1000 + i);
    }
    const auto first_offset = db::page_orders_offset(test.db(), 0, 5);
    const auto first_keyset = db::page_orders_keyset(test.db(), std::nullopt, 5);
    EXPECT_EQ(first_offset.size(), 5u);
    EXPECT_EQ(first_keyset.items, first_offset);

    const auto middle = db::page_orders_offset(test.db(), 5, 5);
    ASSERT_EQ(middle.size(), 5u);
    EXPECT_EQ(middle.front().id, 6);
    EXPECT_EQ(db::page_orders_offset(test.db(), 10, 5).size(), 2u);
}

TEST(Part7Offset, NegativeOffsetAndNonPositiveLimitAreRejected) {
    TestDb test;
    EXPECT_THROW((void)db::page_orders_offset(test.db(), -1, 5), std::invalid_argument);
    EXPECT_THROW((void)db::page_orders_offset(test.db(), 0, 0), std::invalid_argument);
}

TEST(Part7Items, GroupsLinesUnderTheirOrdersInPlacedOrder) {
    TestDb test;
    seed_catalogue(test.db());
    const auto customer = add_customer(test.db(), "a@x.test");
    db::OrderRepository orders(test.db());
    const std::vector<db::OrderLineInput> two_lines{{"WIDGET", 1}, {"GADGET", 2}};
    const std::vector<db::OrderLineInput> one_line{{"GIZMO", 3}};
    const auto later = orders.place(customer, at(200), two_lines);
    const auto earlier = orders.place(customer, at(100), one_line);
    orders.set_status(earlier, "cancelled");  // cancelled orders are still listed

    const auto result = db::load_orders_with_items(test.db(), customer);
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0].order.id, earlier);
    EXPECT_EQ(result[0].order.status, "cancelled");
    ASSERT_EQ(result[0].lines.size(), 1u);
    EXPECT_EQ(result[0].lines[0].sku, "GIZMO");

    EXPECT_EQ(result[1].order.id, later);
    ASSERT_EQ(result[1].lines.size(), 2u);
    EXPECT_EQ(result[1].lines[0].line_no, 1);
    EXPECT_EQ(result[1].lines[0].sku, "WIDGET");
    EXPECT_EQ(result[1].lines[1].line_no, 2);
    EXPECT_EQ(result[1].lines[1].sku, "GADGET");
}

TEST(Part7Items, CustomerWithoutOrdersGetsEmptyResult) {
    TestDb test;
    const auto customer = add_customer(test.db(), "lonely@x.test");
    EXPECT_TRUE(db::load_orders_with_items(test.db(), customer).empty());
}

TEST(Part7Items, StatementCountDoesNotGrowWithOrderCount) {
    TestDb test;
    seed_catalogue(test.db());
    const auto small = add_customer(test.db(), "small@x.test");
    const auto large = add_customer(test.db(), "large@x.test");
    for (int i = 0; i < 3; ++i) {
        place_one(test.db(), small, 100 + i);
    }
    for (int i = 0; i < 40; ++i) {
        place_one(test.db(), large, 100 + i, 2);
    }
    const int small_statements = statements_during(test.db(), [&] {
        (void)db::load_orders_with_items(test.db(), small);
    });
    const int large_statements = statements_during(test.db(), [&] {
        (void)db::load_orders_with_items(test.db(), large);
    });
    EXPECT_EQ(small_statements, large_statements) << "N+1 pattern: per-order queries";
}
