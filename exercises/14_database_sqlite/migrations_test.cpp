// Part 4: versioned migrations via PRAGMA user_version.

#include "migrations.hpp"
#include "sqlite_wrapper.hpp"
#include "test_support_test.hpp"

#include <gtest/gtest.h>
#include <sqlite3.h>

#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace db = cppref::database_sqlite;
using cppref::database_sqlite::testing::TestDb;

namespace {

std::int64_t count_named(db::Database& database, const std::string& name) {
    const auto rows = db::query_all<std::int64_t>(
        database, "SELECT count(*) FROM sqlite_master WHERE name = ?", name);
    return std::get<0>(rows.at(0));
}

}  // namespace

TEST(Part4Migrations, FreshDatabaseReachesLatestVersion) {
    db::Database database(":memory:");
    const int latest = static_cast<int>(db::builtin_migrations().size());
    EXPECT_GE(latest, 2);
    EXPECT_EQ(db::migrate(database), latest);
    EXPECT_EQ(db::schema_version(database), latest);
}

TEST(Part4Migrations, BuiltinSchemaCreatesAllTables) {
    TestDb test;
    for (const char* table : {"customers", "products", "orders", "order_lines"}) {
        EXPECT_EQ(count_named(test.db(), table), 1) << table;
    }
}

TEST(Part4Migrations, RerunningIsANoOp) {
    db::Database database(":memory:");
    const int version = db::migrate(database);
    EXPECT_EQ(db::migrate(database), version);
    EXPECT_EQ(db::schema_version(database), version);
}

TEST(Part4Migrations, AppliesOnlyPendingMigrations) {
    db::Database database(":memory:");
    const std::vector<db::Migration> first{{1, "create t", {"CREATE TABLE t(x INTEGER)"}}};
    const std::vector<db::Migration> all{
        {1, "create t", {"CREATE TABLE t(x INTEGER)"}},
        {2, "add y", {"ALTER TABLE t ADD COLUMN y INTEGER"}},
    };
    EXPECT_EQ(db::migrate(database, first), 1);
    EXPECT_EQ(db::migrate(database, all), 2);
    EXPECT_NO_THROW(database.exec("INSERT INTO t(x, y) VALUES (1, 2)"));
}

TEST(Part4Migrations, RejectsVersionGap) {
    db::Database database(":memory:");
    const std::vector<db::Migration> gap{
        {1, "a", {"CREATE TABLE a(x)"}},
        {3, "c", {"CREATE TABLE c(x)"}},
    };
    EXPECT_THROW((void)db::migrate(database, gap), std::invalid_argument);
}

TEST(Part4Migrations, FailedMigrationRollsBackAndReportsVersion) {
    db::Database database(":memory:");
    const std::vector<db::Migration> bad{
        {1, "ok", {"CREATE TABLE a(x)"}},
        {2, "broken", {"CREATE TABLE b(x)", "SELECT * FROM missing_table"}},
    };
    try {
        (void)db::migrate(database, bad);
        FAIL() << "expected MigrationError";
    } catch (const db::MigrationError& e) {
        EXPECT_EQ(e.version(), 2);
        EXPECT_EQ(e.extended_code(), SQLITE_ERROR);
        try {
            std::rethrow_if_nested(e);
            FAIL() << "expected nested DbError";
        } catch (const db::DbError& inner) {
            EXPECT_EQ(inner.primary_code(), SQLITE_ERROR);
        }
    }
    EXPECT_EQ(db::schema_version(database), 1);
    EXPECT_EQ(count_named(database, "b"), 0);  // v2's CREATE was rolled back with it
    EXPECT_EQ(count_named(database, "a"), 1);
}

TEST(Part4Migrations, NewerDatabaseIsRejected) {
    db::Database database(":memory:");
    database.exec("PRAGMA user_version = 99");
    try {
        (void)db::migrate(database);
        FAIL() << "expected MigrationError";
    } catch (const db::MigrationError& e) {
        EXPECT_EQ(e.version(), 99);
    }
}

TEST(Part4Migrations, BuiltinTablesAreStrictAndCheckedAndAnchored) {
    TestDb test;
    auto& database = test.db();
    try {
        database.exec("INSERT INTO products(sku, name, unit_price_cents) VALUES ('X', 'x', 'cheap')");
        FAIL() << "expected STRICT datatype violation";
    } catch (const db::DbError& e) {
        EXPECT_EQ(e.extended_code(), SQLITE_CONSTRAINT_DATATYPE);
    }
    try {
        database.exec("INSERT INTO products(sku, name, unit_price_cents) VALUES ('X', 'x', -1)");
        FAIL() << "expected CHECK violation";
    } catch (const db::DbError& e) {
        EXPECT_EQ(e.extended_code(), SQLITE_CONSTRAINT_CHECK);
    }
    try {
        database.exec("INSERT INTO orders(customer_id, placed_at, status) VALUES (999, 0, 'pending')");
        FAIL() << "expected foreign key violation";
    } catch (const db::DbError& e) {
        EXPECT_EQ(e.extended_code(), SQLITE_CONSTRAINT_FOREIGNKEY);
    }
}
