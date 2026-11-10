// Parts 1-3: RAII connection/statement wrappers, typed binding, transactions.

#include "sqlite_wrapper.hpp"
#include "test_support_test.hpp"

#include <gtest/gtest.h>
#include <sqlite3.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace db = cppref::database_sqlite;
using cppref::database_sqlite::testing::TempDbFile;

// Compile-time type vocabulary (checked at build time; the stubs compile either way).
static_assert(!db::SqlBindable<bool>, "bool must not bind implicitly");
static_assert(!db::SqlColumnValue<bool>, "bool must not be extracted implicitly");
static_assert(db::SqlBindable<std::optional<double>>, "optional scalars bind as NULL-or-value");
static_assert(!db::SqlExtractable<std::string_view>, "views dangle; extract owning strings");

namespace {

constexpr const char* kSchema = "CREATE TABLE t(id INTEGER PRIMARY KEY, label TEXT, score REAL);";

std::int64_t count_rows(db::Database& database, const std::string& table) {
    const auto rows = db::query_all<std::int64_t>(database, "SELECT count(*) FROM " + table);
    return std::get<0>(rows.at(0));
}

}  // namespace

// ---- Part 1: RAII Database / Statement, errors --------------------------------

TEST(Part1Database, OpensInMemoryAndExecutesDdl) {
    db::Database database(":memory:");
    EXPECT_NE(database.handle(), nullptr);
    EXPECT_NO_THROW(database.exec(kSchema));
    auto stmt = database.prepare("SELECT count(*) FROM sqlite_master WHERE name = 't'");
    ASSERT_TRUE(stmt.step());
    EXPECT_EQ(stmt.column<int>(0), 1);
}

TEST(Part1Database, ExecFailureThrowsDbErrorWithMessage) {
    db::Database database(":memory:");
    try {
        database.exec("SELEC nonsense");
        FAIL() << "expected DbError";
    } catch (const db::DbError& e) {
        EXPECT_EQ(e.primary_code(), SQLITE_ERROR);
        EXPECT_NE(std::string(e.what()).find("syntax error"), std::string::npos);
    }
}

TEST(Part1Database, DbErrorExposesExtendedAndPrimaryCodes) {
    db::Database database(":memory:");
    database.exec("CREATE TABLE p(id INTEGER PRIMARY KEY);"
                  "CREATE TABLE c(pid INTEGER REFERENCES p(id));");
    try {
        database.exec("INSERT INTO c VALUES (42)");
        FAIL() << "expected foreign key violation";
    } catch (const db::DbError& e) {
        EXPECT_EQ(e.extended_code(), SQLITE_CONSTRAINT_FOREIGNKEY);
        EXPECT_EQ(e.primary_code(), SQLITE_CONSTRAINT);
    }
}

TEST(Part1Database, UniqueViolationIsExtendedConstraintUnique) {
    db::Database database(":memory:");
    database.exec("CREATE TABLE u(k TEXT UNIQUE); INSERT INTO u VALUES ('a');");
    try {
        database.exec("INSERT INTO u VALUES ('a')");
        FAIL() << "expected unique violation";
    } catch (const db::DbError& e) {
        EXPECT_EQ(e.extended_code(), SQLITE_CONSTRAINT_UNIQUE);
    }
}

TEST(Part1Database, TryFormsReportFailuresAsValues) {
    db::Database database(":memory:");
    const auto exec_result = database.try_exec("SELEC");
    ASSERT_FALSE(exec_result.has_value());
    EXPECT_EQ(exec_result.error().code, SQLITE_ERROR);
    EXPECT_FALSE(exec_result.error().message.empty());
    EXPECT_TRUE(database.try_exec("SELECT 1").has_value());

    auto prepared = database.try_prepare("SELECT * FROM missing_table");
    ASSERT_FALSE(prepared.has_value());
    EXPECT_EQ(prepared.error().extended_code, SQLITE_ERROR);
}

TEST(Part1Database, PreparedStatementStepsAndResets) {
    db::Database database(":memory:");
    database.exec(kSchema);
    database.exec("INSERT INTO t(label, score) VALUES ('a', 1.5), ('b', 2.5)");

    auto stmt = database.prepare("SELECT id, label FROM t ORDER BY id");
    EXPECT_EQ(stmt.column_count(), 2);
    ASSERT_TRUE(stmt.step());
    EXPECT_EQ(stmt.column<std::int64_t>(0), 1);
    EXPECT_EQ(stmt.column<std::string>(1), "a");
    ASSERT_TRUE(stmt.step());
    EXPECT_EQ(stmt.column<std::string>(1), "b");
    EXPECT_FALSE(stmt.step());

    stmt.reset();
    ASSERT_TRUE(stmt.step());
    EXPECT_EQ(stmt.column<std::string>(1), "a");
}

TEST(Part1Database, MoveTransfersOwnershipAndEmptiesSource) {
    db::Database first(":memory:");
    db::Database second(std::move(first));
    EXPECT_EQ(first.handle(), nullptr);
    EXPECT_NE(second.handle(), nullptr);

    db::Database third(":memory:");
    third = std::move(second);
    EXPECT_EQ(second.handle(), nullptr);
    EXPECT_NE(third.handle(), nullptr);

    auto stmt = third.prepare("SELECT 1");
    db::Statement moved(std::move(stmt));
    EXPECT_EQ(stmt.handle(), nullptr);
    ASSERT_TRUE(moved.step());
    EXPECT_EQ(moved.column<int>(0), 1);
}

TEST(Part1Database, ChangesAndLastInsertRowidTrackWrites) {
    db::Database database(":memory:");
    database.exec(kSchema);
    database.exec("INSERT INTO t(label) VALUES ('x')");
    EXPECT_EQ(database.last_insert_rowid(), 1);
    database.exec("UPDATE t SET label = 'y'");
    EXPECT_EQ(database.changes(), 1);
}

TEST(Part1Database, FileDatabasePersistsAcrossReopen) {
    TempDbFile file;  // declared first: removed after the connections close
    {
        db::Database writer(file.string());
        writer.exec(kSchema);
        writer.exec("INSERT INTO t(label) VALUES ('kept')");
    }
    db::Database reader(file.string());
    auto stmt = reader.prepare("SELECT label FROM t");
    ASSERT_TRUE(stmt.step());
    EXPECT_EQ(stmt.column<std::string>(0), "kept");
}

// ---- Part 2: typed binding and extraction ---------------------------------------

TEST(Part2Binding, BindsIntegersDoublesAndText) {
    db::Database database(":memory:");
    auto stmt = database.prepare("SELECT ?, ?, ?");
    stmt.bind(1, std::int64_t{42});
    stmt.bind(2, 2.5);
    stmt.bind(3, std::string_view{"txt"});
    ASSERT_TRUE(stmt.step());
    EXPECT_EQ(stmt.column<std::int64_t>(0), 42);
    EXPECT_DOUBLE_EQ(stmt.column<double>(1), 2.5);
    EXPECT_EQ(stmt.column<std::string>(2), "txt");
}

TEST(Part2Binding, OptionalBindsNullAndValue) {
    db::Database database(":memory:");
    auto stmt = database.prepare("SELECT ?, ?");
    stmt.bind(1, std::optional<std::int64_t>{});
    stmt.bind(2, std::optional<std::string_view>{"x"});
    ASSERT_TRUE(stmt.step());
    EXPECT_EQ(stmt.column<std::optional<std::int64_t>>(0), std::nullopt);
    EXPECT_EQ(stmt.column<std::optional<std::string>>(1), std::optional<std::string>{"x"});
}

TEST(Part2Binding, NullIntoNonOptionalColumnThrowsMismatch) {
    db::Database database(":memory:");
    auto stmt = database.prepare("SELECT NULL");
    ASSERT_TRUE(stmt.step());
    try {
        (void)stmt.column<std::int64_t>(0);
        FAIL() << "expected SQLITE_MISMATCH";
    } catch (const db::DbError& e) {
        EXPECT_EQ(e.extended_code(), SQLITE_MISMATCH);
    }
}

TEST(Part2Binding, RowReadsTupleOfMixedTypes) {
    db::Database database(":memory:");
    auto stmt = database.prepare("SELECT 7, 'seven', 7.5");
    ASSERT_TRUE(stmt.step());
    const auto row = stmt.row<std::int64_t, std::string, double>();
    EXPECT_EQ(std::get<0>(row), 7);
    EXPECT_EQ(std::get<1>(row), "seven");
    EXPECT_DOUBLE_EQ(std::get<2>(row), 7.5);
}

TEST(Part2Binding, BoundTextIsCopiedBeforeCallerBufferChanges) {
    db::Database database(":memory:");
    auto stmt = database.prepare("SELECT ?");
    std::string buffer = "transient";
    stmt.bind(1, std::string_view{buffer});
    buffer.assign("overwritten");  // SQLITE_TRANSIENT: the statement keeps its own copy
    ASSERT_TRUE(stmt.step());
    EXPECT_EQ(stmt.column<std::string>(0), "transient");
}

TEST(Part2Binding, BindAllBindsInOrderAndClearBindingsSetsNull) {
    db::Database database(":memory:");
    auto stmt = database.prepare("SELECT ?, ?");
    stmt.bind_all(std::int64_t{1}, std::string_view{"two"});
    ASSERT_TRUE(stmt.step());
    EXPECT_EQ(stmt.column<std::int64_t>(0), 1);
    EXPECT_EQ(stmt.column<std::string>(1), "two");

    stmt.reset();
    stmt.clear_bindings();
    ASSERT_TRUE(stmt.step());
    EXPECT_EQ(stmt.column<std::optional<std::int64_t>>(0), std::nullopt);
}

TEST(Part2Query, QueryAllBindsArgumentsAndCollectsRows) {
    db::Database database(":memory:");
    database.exec(kSchema);
    database.exec("INSERT INTO t(label, score) VALUES ('a', 1.0), ('b', 2.0), ('c', 3.0)");

    const auto rows = db::query_all<std::int64_t, std::string>(
        database, "SELECT id, label FROM t WHERE score >= ? ORDER BY id", 2.0);
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(std::get<1>(rows[0]), "b");
    EXPECT_EQ(std::get<1>(rows[1]), "c");

    EXPECT_TRUE(db::query_all<std::int64_t>(database, "SELECT id FROM t WHERE 0").empty());
}

// ---- Part 3: Transaction and Savepoint ------------------------------------------

TEST(Part3Transaction, CommitPersistsAndClearsTransactionState) {
    TempDbFile file;
    db::Database database(file.string());
    database.exec(kSchema);
    {
        db::Transaction tx(database);
        EXPECT_TRUE(database.in_transaction());
        database.exec("INSERT INTO t(label) VALUES ('committed')");
        tx.commit();
    }
    EXPECT_FALSE(database.in_transaction());
    EXPECT_EQ(count_rows(database, "t"), 1);
}

TEST(Part3Transaction, DestructorRollsBackWithoutCommit) {
    db::Database database(":memory:");
    database.exec(kSchema);
    {
        db::Transaction tx(database);
        database.exec("INSERT INTO t(label) VALUES ('lost')");
    }
    EXPECT_EQ(count_rows(database, "t"), 0);
}

TEST(Part3Transaction, ExceptionInsideScopeRollsBack) {
    db::Database database(":memory:");
    database.exec(kSchema);
    EXPECT_THROW(
        {
            db::Transaction tx(database);
            database.exec("INSERT INTO t(label) VALUES ('lost')");
            throw std::runtime_error("boom");
        },
        std::runtime_error);
    EXPECT_EQ(count_rows(database, "t"), 0);
}

TEST(Part3Transaction, SecondWriterGetsSqliteBusy) {
    TempDbFile file;
    db::Database holder(file.string());
    holder.exec(kSchema);
    db::Database other(file.string());
    holder.exec("BEGIN IMMEDIATE");
    try {
        db::Transaction tx(other);
        FAIL() << "expected SQLITE_BUSY";
    } catch (const db::DbError& e) {
        EXPECT_EQ(e.primary_code(), SQLITE_BUSY);
    }
    holder.exec("ROLLBACK");
}

TEST(Part3Savepoint, InnerRollbackKeepsOuterWork) {
    db::Database database(":memory:");
    database.exec(kSchema);
    db::Transaction tx(database);
    database.exec("INSERT INTO t(label) VALUES ('outer')");
    {
        db::Savepoint sp(database, "inner");
        database.exec("INSERT INTO t(label) VALUES ('inner')");
        // not released: destructor rolls back to the savepoint
    }
    tx.commit();

    auto rows = db::query_all<std::string>(database, "SELECT label FROM t");
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(std::get<0>(rows[0]), "outer");
}

TEST(Part3Savepoint, ReleasedSavepointSurvivesOuterCommit) {
    db::Database database(":memory:");
    database.exec(kSchema);
    db::Transaction tx(database);
    {
        db::Savepoint sp(database, "keep");
        database.exec("INSERT INTO t(label) VALUES ('kept')");
        sp.release();
    }
    tx.commit();
    EXPECT_EQ(count_rows(database, "t"), 1);
}

TEST(Part3Savepoint, RejectsNonIdentifierName) {
    db::Database database(":memory:");
    EXPECT_THROW((void)db::Savepoint(database, "bad name; DROP TABLE t"), std::invalid_argument);
    EXPECT_THROW((void)db::Savepoint(database, "1starts_with_digit"), std::invalid_argument);
}
