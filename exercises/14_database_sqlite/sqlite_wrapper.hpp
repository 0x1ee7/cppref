#pragma once

#include <concepts>
#include <cstdint>
#include <expected>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

// Forward declarations of the SQLite C types (global scope, same tags as sqlite3.h).
struct sqlite3;
struct sqlite3_stmt;

namespace cppref::database_sqlite {

// Snapshot of a failed SQLite call.
struct SqlError {
    int code = 0;           // primary result code, e.g. SQLITE_CONSTRAINT (19)
    int extended_code = 0;  // extended result code, e.g. SQLITE_CONSTRAINT_UNIQUE (2067)
    std::string message;    // connection message (sqlite3_errmsg) or sqlite3_errstr
};

// Exception form of a SQLite failure. what() is the message; extended_code() is the
// full extended result code. Engineer-owned: store the extended code as a private member.
class DbError : public std::runtime_error {
public:
    DbError(int extended_code, const std::string& message);

    // Full extended result code, e.g. SQLITE_CONSTRAINT_FOREIGNKEY (787).
    int extended_code() const noexcept;
    // Primary code: extended_code() & 0xFF, e.g. SQLITE_CONSTRAINT (19).
    int primary_code() const noexcept;
};

// ---- Type vocabulary ------------------------------------------------------------

template <class T>
struct is_optional : std::false_type {};
template <class T>
struct is_optional<std::optional<T>> : std::true_type {};

// Non-bool integers (INTEGER), floating point (REAL), and text views (TEXT).
// bool is deliberately not bindable: it hides a 0/1 decision behind a type.
template <class T>
concept SqlScalar = (std::integral<T> && !std::same_as<T, bool>) ||
                    std::floating_point<T> ||
                    std::convertible_to<T, std::string_view>;

// Bindable: a scalar, or std::optional of a scalar (nullopt binds NULL).
template <class T>
concept SqlBindable = SqlScalar<T> ||
                      (is_optional<T>::value && SqlScalar<typename T::value_type>);

// Extractable from a column: integers, double, owning std::string (no views: they dangle).
template <class T>
concept SqlExtractable = (std::integral<T> && !std::same_as<T, bool>) ||
                         std::floating_point<T> || std::same_as<T, std::string>;

// Column type: extractable, or std::optional of extractable (NULL -> nullopt).
template <class T>
concept SqlColumnValue = SqlExtractable<T> ||
                         (is_optional<T>::value && SqlExtractable<typename T::value_type>);

// ---- Statement ------------------------------------------------------------------

// Move-only owner of a prepared sqlite3_stmt; finalized on destruction.
// Created by Database::prepare.
class Statement {
public:
    // Adopts stmt (may be null for empty SQL). Does not throw.
    explicit Statement(sqlite3_stmt* stmt) noexcept;

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    // Transfers the statement; the source owns nothing afterwards. Does not throw.
    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&& other) noexcept;
    // Finalizes the statement. Never throws.
    ~Statement();

    // Binds value to the 1-based parameter index. Text is copied (SQLITE_TRANSIENT),
    // so the caller's buffer may die right after the call. nullopt binds NULL.
    // Throws DbError on failure (for example SQLITE_RANGE for a bad index).
    template <SqlBindable T>
    void bind(int index, const T& value) {
        (void)index;
        (void)value;
        throw std::logic_error("not implemented: Statement::bind");
    }

    // Binds the arguments to parameters 1..sizeof...(Args), in order.
    template <SqlBindable... Args>
    void bind_all(const Args&... args) {
        (void)sizeof...(args);
        ((void)args, ...);
        throw std::logic_error("not implemented: Statement::bind_all");
    }

    // Advances to the next row. true: a row is available; false: SQLITE_DONE.
    // Throws DbError on failure.
    bool step();

    // Rewinds to the first row, keeping bindings. Does not throw: a failing step
    // has already been reported by step().
    void reset() noexcept;

    // Clears all parameter bindings to NULL. Does not throw.
    void clear_bindings() noexcept;

    // Number of result columns. Does not throw.
    int column_count() const noexcept;

    // Reads a 0-based column. A NULL in a non-optional T throws DbError(SQLITE_MISMATCH).
    template <SqlColumnValue T>
    T column(int index) const {
        (void)index;
        throw std::logic_error("not implemented: Statement::column");
    }

    // Reads columns 0..sizeof...(Ts)-1 of the current row as a tuple.
    template <SqlColumnValue... Ts>
    std::tuple<Ts...> row() const {
        throw std::logic_error("not implemented: Statement::row");
    }

    // Underlying statement; non-owning. Does not throw.
    sqlite3_stmt* handle() const noexcept;

private:
    sqlite3_stmt* stmt_ = nullptr;  // engineer-owned member
};

// ---- Database -------------------------------------------------------------------

// Move-only owner of a sqlite3 connection, closed with sqlite3_close_v2.
class Database {
public:
    // Opens or creates the database file (":memory:" for a private in-memory database).
    // Enables extended result codes and PRAGMA foreign_keys = ON. Throws DbError.
    explicit Database(const std::string& path);

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    // Transfers the connection; the source owns nothing afterwards. Does not throw.
    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;
    // Closes the connection. Never throws.
    ~Database();

    // Runs one or more statements that take no parameters. Throws DbError.
    void exec(std::string_view sql);

    // Same as exec, but reports failure as a value.
    std::expected<void, SqlError> try_exec(std::string_view sql);

    // Prepares a single statement. Throws DbError.
    Statement prepare(std::string_view sql);

    // Same as prepare, but reports failure as a value.
    std::expected<Statement, SqlError> try_prepare(std::string_view sql);

    // rowid of the most recent successful INSERT on this connection. Does not throw.
    std::int64_t last_insert_rowid() const noexcept;

    // Rows changed by the most recent INSERT/UPDATE/DELETE. Does not throw.
    int changes() const noexcept;

    // True while a transaction is open (autocommit disabled). Does not throw.
    bool in_transaction() const noexcept;

    // Non-owning escape hatch for APIs the wrapper does not cover (trace hooks, backup).
    // Null after the database was moved from. Never close it through this pointer.
    sqlite3* handle() const noexcept;

private:
    sqlite3* db_ = nullptr;  // engineer-owned member
};

// ---- Transactions ---------------------------------------------------------------

// BEGIN IMMEDIATE in the constructor (throws DbError, e.g. SQLITE_BUSY).
// commit() issues COMMIT. The destructor rolls back unless committed and never throws.
// Non-copyable, non-movable.
class Transaction {
public:
    explicit Transaction(Database& db);
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    Transaction(Transaction&&) = delete;
    Transaction& operator=(Transaction&&) = delete;
    ~Transaction();

    // Throws DbError if COMMIT fails; the destructor then rolls back.
    void commit();

private:
    Database* db_ = nullptr;  // engineer-owned member
};

// SAVEPOINT name in the constructor. release() issues RELEASE. The destructor, when not
// released, issues ROLLBACK TO name followed by RELEASE name and never throws.
// name must be a plain identifier ([A-Za-z_][A-Za-z0-9_]*), else std::invalid_argument.
// Non-copyable, non-movable.
class Savepoint {
public:
    Savepoint(Database& db, std::string_view name);
    Savepoint(const Savepoint&) = delete;
    Savepoint& operator=(const Savepoint&) = delete;
    Savepoint(Savepoint&&) = delete;
    Savepoint& operator=(Savepoint&&) = delete;
    ~Savepoint();

    // Throws DbError if RELEASE fails.
    void release();

private:
    Database* db_ = nullptr;  // engineer-owned member
    std::string name_;        // engineer-owned member
};

// ---- Convenience ----------------------------------------------------------------

// Prepares sql, binds args to its parameters, and collects every row as a tuple.
template <SqlColumnValue... Ts, SqlBindable... Args>
std::vector<std::tuple<Ts...>> query_all(Database& db, std::string_view sql, const Args&... args) {
    (void)db;
    (void)sql;
    ((void)args, ...);
    throw std::logic_error("not implemented: query_all");
}

}  // namespace cppref::database_sqlite
