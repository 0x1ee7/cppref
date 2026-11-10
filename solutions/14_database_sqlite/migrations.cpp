#include "migrations.hpp"
#include <sqlite3.h>

#include <exception>
#include <stdexcept>
#include <string>

namespace cppref::database_sqlite {

MigrationError::MigrationError(int version, int extended_code, const std::string& message)
    : std::runtime_error(message), version_(version), extended_code_(extended_code) {}

int MigrationError::version() const noexcept { return version_; }

int MigrationError::extended_code() const noexcept { return extended_code_; }

std::span<const Migration> builtin_migrations() {
    static const std::vector<Migration> kBuiltin = {
        {1,
         "create core tables",
         {
             "CREATE TABLE customers("
             "  id INTEGER PRIMARY KEY,"
             "  email TEXT NOT NULL UNIQUE COLLATE NOCASE,"
             "  name TEXT NOT NULL,"
             "  created_at INTEGER NOT NULL) STRICT",
             "CREATE TABLE products("
             "  sku TEXT PRIMARY KEY,"
             "  name TEXT NOT NULL,"
             "  unit_price_cents INTEGER NOT NULL CHECK (unit_price_cents >= 0)) STRICT",
             "CREATE TABLE orders("
             "  id INTEGER PRIMARY KEY,"
             "  customer_id INTEGER NOT NULL REFERENCES customers(id),"
             "  placed_at INTEGER NOT NULL,"
             "  status TEXT NOT NULL CHECK (status IN ('pending','paid','shipped','cancelled'))) STRICT",
             "CREATE TABLE order_lines("
             "  order_id INTEGER NOT NULL REFERENCES orders(id),"
             "  line_no INTEGER NOT NULL,"
             "  sku TEXT NOT NULL REFERENCES products(sku),"
             "  quantity INTEGER NOT NULL CHECK (quantity > 0),"
             "  unit_price_cents INTEGER NOT NULL CHECK (unit_price_cents >= 0),"
             "  PRIMARY KEY (order_id, line_no)) STRICT",
         }},
        {2,
         "index orders by customer and time",
         {"CREATE INDEX orders_customer_placed ON orders(customer_id, placed_at)"}},
    };
    return kBuiltin;
}

int schema_version(Database& db) {
    Statement stmt = db.prepare("PRAGMA user_version");
    if (!stmt.step()) {
        throw DbError(SQLITE_ERROR, "PRAGMA user_version returned no row");
    }
    return stmt.column<int>(0);
}

int migrate(Database& db, std::span<const Migration> migrations) {
    for (std::size_t i = 0; i < migrations.size(); ++i) {
        if (migrations[i].version != static_cast<int>(i) + 1) {
            throw std::invalid_argument("migration versions must be exactly 1..n in order");
        }
    }

    const int current = schema_version(db);
    const int latest = static_cast<int>(migrations.size());
    if (current > latest) {
        throw MigrationError(current, 0, "database schema is newer than this build");
    }

    for (const Migration& migration : migrations) {
        if (migration.version <= current) {
            continue;
        }
        try {
            // The Transaction rolls back during unwinding, before the catch below runs.
            Transaction tx(db);
            for (const std::string& statement : migration.statements) {
                db.exec(statement);
            }
            db.exec("PRAGMA user_version = " + std::to_string(migration.version));
            tx.commit();
        } catch (const DbError& e) {
            std::throw_with_nested(
                MigrationError(migration.version, e.extended_code(), e.what()));
        }
    }
    return schema_version(db);
}

}  // namespace cppref::database_sqlite
