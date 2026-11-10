#pragma once

#include "sqlite_wrapper.hpp"

#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace cppref::database_sqlite {

struct Migration {
    int version = 0;                         // must be 1..n, in order
    std::string name;
    std::vector<std::string> statements;     // applied in order inside one transaction
};

// Thrown when a migration fails (its transaction has been rolled back) or when the
// database is newer than the code. The failing DbError, if any, is attached as a
// nested exception (throw_with_nested), so std::rethrow_if_nested recovers it.
class MigrationError : public std::runtime_error {
public:
    MigrationError(int version, int extended_code, const std::string& message);

    // Version of the migration that failed; for a too-new database, its user_version.
    int version() const noexcept;
    // SQLite extended code of the failing statement; 0 when not caused by SQLite.
    int extended_code() const noexcept;

    // Engineer-owned: store version and extended code as private members.
};

// The shipped schema (all STRICT, foreign keys enforced). Column names are part of the contract:
//   customers(id INTEGER PK, email TEXT UNIQUE COLLATE NOCASE, name TEXT, created_at INTEGER)
//   products(sku TEXT PK, name TEXT, unit_price_cents INTEGER CHECK >= 0)
//   orders(id INTEGER PK, customer_id -> customers, placed_at INTEGER,
//          status TEXT CHECK IN ('pending','paid','shipped','cancelled'))
//   order_lines(order_id -> orders, line_no, sku -> products, quantity CHECK > 0,
//               unit_price_cents, PK(order_id, line_no))
// v2: index on orders(customer_id, placed_at). Returned as a span over static storage.
std::span<const Migration> builtin_migrations();

// PRAGMA user_version. Throws DbError.
int schema_version(Database& db);

// Applies every migration with version > PRAGMA user_version. Each runs in its own
// BEGIN IMMEDIATE transaction together with the user_version bump.
// Throws std::invalid_argument if the versions are not exactly 1..n in order.
// Throws MigrationError on a failing migration or a database newer than the code.
// Returns the resulting user_version. Re-running is a no-op.
int migrate(Database& db, std::span<const Migration> migrations = builtin_migrations());

}  // namespace cppref::database_sqlite
