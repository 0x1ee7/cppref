// Parts 5-6: repositories with parameterised queries; bulk import and upsert.

#include "model.hpp"
#include "repositories.hpp"
#include "sqlite_wrapper.hpp"
#include "test_support_test.hpp"

#include <gtest/gtest.h>
#include <sqlite3.h>

#include <cstdint>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace db = cppref::database_sqlite;
using cppref::database_sqlite::testing::TestDb;
using cppref::database_sqlite::testing::at;
using cppref::database_sqlite::testing::seed_catalogue;

namespace {

std::int64_t count_rows(db::Database& database, const std::string& table) {
    const auto rows = db::query_all<std::int64_t>(database, "SELECT count(*) FROM " + table);
    return std::get<0>(rows.at(0));
}

std::vector<db::Product> make_products(int n, std::int64_t price) {
    std::vector<db::Product> rows;
    rows.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        rows.push_back({"P" + std::to_string(i), "Product " + std::to_string(i), db::Money{price}});
    }
    return rows;
}

}  // namespace

// ---- Part 5: repositories -------------------------------------------------------

TEST(Part5Customers, AddThenGetRoundTrips) {
    TestDb test;
    db::CustomerRepository customers(test.db());
    const auto added = customers.add("ada@example.com", "Ada Lovelace", at(1000));
    EXPECT_GT(added.id, 0);
    const auto fetched = customers.get(added.id);
    ASSERT_TRUE(fetched.has_value());
    EXPECT_EQ(*fetched, added);
    EXPECT_FALSE(customers.get(9999).has_value());
}

TEST(Part5Customers, DuplicateEmailIgnoringCaseIsUniqueViolation) {
    TestDb test;
    db::CustomerRepository customers(test.db());
    customers.add("ada@example.com", "Ada", at(1));
    try {
        (void)customers.add("ADA@example.com", "Other", at(2));
        FAIL() << "expected unique violation";
    } catch (const db::DbError& e) {
        EXPECT_EQ(e.extended_code(), SQLITE_CONSTRAINT_UNIQUE);
    }
    EXPECT_EQ(count_rows(test.db(), "customers"), 1);
}

TEST(Part5Customers, EmailLookupIsCaseInsensitive) {
    TestDb test;
    db::CustomerRepository customers(test.db());
    const auto added = customers.add("ada@example.com", "Ada", at(1));
    const auto found = customers.get_by_email("ADA@Example.COM");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->id, added.id);
}

TEST(Part5Customers, SearchIsLiteralAndCaseInsensitive) {
    TestDb test;
    db::CustomerRepository customers(test.db());
    customers.add("a@x.test", "Alice Smith", at(1));
    customers.add("b@x.test", "Bob", at(2));
    customers.add("c@x.test", "100% Done_Ltd", at(3));

    const auto alice = customers.search_by_name("ALI");
    ASSERT_EQ(alice.size(), 1u);
    EXPECT_EQ(alice[0].name, "Alice Smith");

    const auto percent = customers.search_by_name("%");
    ASSERT_EQ(percent.size(), 1u);
    EXPECT_EQ(percent[0].name, "100% Done_Ltd");

    const auto underscore = customers.search_by_name("_");
    ASSERT_EQ(underscore.size(), 1u);
    EXPECT_EQ(underscore[0].name, "100% Done_Ltd");

    EXPECT_THROW((void)customers.search_by_name(""), std::invalid_argument);
}

TEST(Part5Products, UpsertInsertsThenReplaces) {
    TestDb test;
    db::ProductRepository products(test.db());
    products.upsert(db::Product{"WIDGET", "Widget", db::Money{1000}});
    products.upsert(db::Product{"WIDGET", "Widget v2", db::Money{1200}});

    const auto got = products.get("WIDGET");
    ASSERT_TRUE(got.has_value());
    EXPECT_EQ(got->name, "Widget v2");
    EXPECT_EQ(got->unit_price, db::Money{1200});
    EXPECT_FALSE(products.get("NOPE").has_value());
    EXPECT_EQ(count_rows(test.db(), "products"), 1);
}

TEST(Part5Products, NegativePriceIsRejected) {
    TestDb test;
    db::ProductRepository products(test.db());
    const db::Product bad{"BAD", "bad", db::Money{-1}};
    EXPECT_THROW(products.upsert(bad), std::invalid_argument);
    EXPECT_FALSE(products.get("BAD").has_value());
}

TEST(Part5Products, UpsertManyStoresEveryRow) {
    TestDb test;
    db::ProductRepository products(test.db());
    const std::vector<db::Product> rows{
        {"A", "a", db::Money{1}},
        {"B", "b", db::Money{2}},
    };
    products.upsert_many(rows);
    EXPECT_EQ(count_rows(test.db(), "products"), 2);
}

TEST(Part5Products, UpsertManyIsAllOrNothing) {
    TestDb test;
    db::ProductRepository products(test.db());
    const std::vector<db::Product> rows{
        {"A", "a", db::Money{1}},
        {"B", "b", db::Money{-5}},
    };
    EXPECT_THROW(products.upsert_many(rows), std::invalid_argument);
    EXPECT_FALSE(products.get("A").has_value());
    EXPECT_EQ(count_rows(test.db(), "products"), 0);
}

TEST(Part5Orders, PlaceSnapshotsPricesAndNumbersLines) {
    TestDb test;
    seed_catalogue(test.db());
    const auto alice = db::CustomerRepository(test.db()).add("alice@x.test", "Alice", at(1)).id;
    db::ProductRepository products(test.db());
    db::OrderRepository orders(test.db());

    const std::vector<db::OrderLineInput> lines{{"GADGET", 2}, {"WIDGET", 1}};
    const auto id = orders.place(alice, at(500), lines);

    const auto header = orders.get(id);
    ASSERT_TRUE(header.has_value());
    EXPECT_EQ(header->customer_id, alice);
    EXPECT_EQ(header->status, "pending");
    EXPECT_EQ(header->placed_at, at(500));

    // Later catalogue changes must not alter the snapshot taken at placement.
    products.upsert(db::Product{"WIDGET", "Widget", db::Money{9999}});

    const auto stored = orders.lines(id);
    ASSERT_EQ(stored.size(), 2u);
    EXPECT_EQ(stored[0], (db::OrderLine{id, 1, "GADGET", 2, db::Money{2550}}));
    EXPECT_EQ(stored[1], (db::OrderLine{id, 2, "WIDGET", 1, db::Money{1000}}));
}

TEST(Part5Orders, PlaceRejectsEmptyOrNonPositiveQuantities) {
    TestDb test;
    seed_catalogue(test.db());
    const auto alice = db::CustomerRepository(test.db()).add("alice@x.test", "Alice", at(1)).id;
    db::OrderRepository orders(test.db());

    const std::vector<db::OrderLineInput> none;
    EXPECT_THROW((void)orders.place(alice, at(1), none), std::invalid_argument);
    const std::vector<db::OrderLineInput> zero{{"WIDGET", 0}};
    EXPECT_THROW((void)orders.place(alice, at(1), zero), std::invalid_argument);
    EXPECT_EQ(count_rows(test.db(), "orders"), 0);
}

TEST(Part5Orders, UnknownSkusAreReportedSortedAndUnique) {
    TestDb test;
    seed_catalogue(test.db());
    const auto alice = db::CustomerRepository(test.db()).add("alice@x.test", "Alice", at(1)).id;
    db::OrderRepository orders(test.db());

    const std::vector<db::OrderLineInput> lines{{"NOPE", 1}, {"WIDGET", 1}, {"ZZZ", 2}, {"NOPE", 3}};
    try {
        (void)orders.place(alice, at(1), lines);
        FAIL() << "expected UnknownProductError";
    } catch (const db::UnknownProductError& e) {
        EXPECT_EQ(e.skus(), (std::vector<std::string>{"NOPE", "ZZZ"}));
    }
    EXPECT_EQ(count_rows(test.db(), "orders"), 0);
    EXPECT_EQ(count_rows(test.db(), "order_lines"), 0);
}

TEST(Part5Orders, UnknownCustomerFailsAtomically) {
    TestDb test;
    seed_catalogue(test.db());
    db::OrderRepository orders(test.db());
    const std::vector<db::OrderLineInput> lines{{"WIDGET", 1}};
    try {
        (void)orders.place(9999, at(1), lines);
        FAIL() << "expected foreign key violation";
    } catch (const db::DbError& e) {
        EXPECT_EQ(e.extended_code(), SQLITE_CONSTRAINT_FOREIGNKEY);
    }
    EXPECT_EQ(count_rows(test.db(), "orders"), 0);
    EXPECT_EQ(count_rows(test.db(), "order_lines"), 0);
}

TEST(Part5Orders, SetStatusValidatesStatusAndOrderId) {
    TestDb test;
    seed_catalogue(test.db());
    const auto alice = db::CustomerRepository(test.db()).add("alice@x.test", "Alice", at(1)).id;
    db::OrderRepository orders(test.db());
    const std::vector<db::OrderLineInput> lines{{"WIDGET", 1}};
    const auto id = orders.place(alice, at(1), lines);

    orders.set_status(id, "paid");
    EXPECT_EQ(orders.get(id)->status, "paid");
    EXPECT_THROW(orders.set_status(id, "lost"), std::invalid_argument);
    EXPECT_THROW(orders.set_status(9999, "paid"), std::out_of_range);
}

TEST(Part5Security, InjectionAttemptsAreInert) {
    TestDb test;
    seed_catalogue(test.db());
    db::CustomerRepository customers(test.db());
    const std::string evil = "x'); DROP TABLE customers; --@example.com";
    customers.add(evil, "Bobby Tables", at(1));

    const auto found = customers.get_by_email(evil);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->name, "Bobby Tables");
    EXPECT_TRUE(customers.search_by_name("' OR 1=1 --").empty());
    EXPECT_EQ(count_rows(test.db(), "customers"), 1);
}

// ---- Part 6: bulk import and upsert ---------------------------------------------

TEST(Part6Import, ImportsAllRowsAcrossBatches) {
    TestDb test;
    const auto rows = make_products(1000, 100);
    EXPECT_EQ(db::import_products(test.db(), rows, 100), 1000u);
    EXPECT_EQ(count_rows(test.db(), "products"), 1000);
    EXPECT_EQ(db::ProductRepository(test.db()).get("P999")->unit_price, db::Money{100});
}

TEST(Part6Import, ReimportUpdatesPricesWithoutDuplicates) {
    TestDb test;
    const auto first = make_products(50, 100);
    const auto second = make_products(50, 200);
    db::import_products(test.db(), first, 10);
    db::import_products(test.db(), second, 10);
    EXPECT_EQ(count_rows(test.db(), "products"), 50);
    EXPECT_EQ(db::ProductRepository(test.db()).get("P7")->unit_price, db::Money{200});
}

TEST(Part6Import, ZeroBatchSizeIsRejected) {
    TestDb test;
    const auto rows = make_products(3, 1);
    EXPECT_THROW((void)db::import_products(test.db(), rows, 0), std::invalid_argument);
}

TEST(Part6Import, BadRowRollsBackOnlyItsOwnBatch) {
    TestDb test;
    auto rows = make_products(300, 100);
    rows[250].unit_price = db::Money{-1};  // lands in the third batch of 100
    EXPECT_THROW((void)db::import_products(test.db(), rows, 100), std::invalid_argument);

    // Batches 0 and 1 were committed; batch 2 (rows 200..299) rolled back as a whole.
    EXPECT_EQ(count_rows(test.db(), "products"), 200);
    db::ProductRepository products(test.db());
    EXPECT_TRUE(products.get("P199").has_value());
    EXPECT_FALSE(products.get("P200").has_value());
}

TEST(Part6Import, AcceptsSingleUsePipelineRanges) {
    TestDb test;
    const auto source = make_products(5, 1);
    auto pipeline = source | std::views::transform([](const db::Product& p) { return p; });
    EXPECT_EQ(db::import_products(test.db(), pipeline, 2), 5u);
    EXPECT_EQ(count_rows(test.db(), "products"), 5);
}
