#include "sqlite_wrapper.hpp"

#include <sqlite3.h>

#include <stdexcept>
#include <utility>

namespace cppref::database_sqlite {

namespace {

[[noreturn]] void throw_last_error(sqlite3* db) {
    if (db == nullptr) {
        throw DbError(SQLITE_MISUSE, "operation on an empty or moved-from handle");
    }
    throw DbError(sqlite3_extended_errcode(db), sqlite3_errmsg(db));
}

std::string checked_savepoint_name(std::string_view name) {
    auto ident_char = [](char c, bool first) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
               (!first && c >= '0' && c <= '9');
    };
    if (name.empty() || !ident_char(name.front(), true)) {
        throw std::invalid_argument("savepoint name must be an identifier");
    }
    for (char c : name.substr(1)) {
        if (!ident_char(c, false)) {
            throw std::invalid_argument("savepoint name must be an identifier");
        }
    }
    return std::string(name);
}

}  // namespace

// ---- DbError --------------------------------------------------------------------

DbError::DbError(int extended_code, const std::string& message)
    : std::runtime_error(message), extended_code_(extended_code) {}

int DbError::extended_code() const noexcept { return extended_code_; }

int DbError::primary_code() const noexcept { return extended_code_ & 0xFF; }

// ---- detail ---------------------------------------------------------------------

namespace detail {

void bind_int64(sqlite3_stmt* stmt, int index, std::int64_t value) {
    if (sqlite3_bind_int64(stmt, index, value) != SQLITE_OK) {
        throw_last_error(sqlite3_db_handle(stmt));
    }
}

void bind_double(sqlite3_stmt* stmt, int index, double value) {
    if (sqlite3_bind_double(stmt, index, value) != SQLITE_OK) {
        throw_last_error(sqlite3_db_handle(stmt));
    }
}

void bind_text(sqlite3_stmt* stmt, int index, std::string_view value) {
    // A null data pointer would bind NULL, so substitute a valid empty string.
    const char* data = value.empty() ? "" : value.data();
    if (sqlite3_bind_text(stmt, index, data, static_cast<int>(value.size()), SQLITE_TRANSIENT) !=
        SQLITE_OK) {
        throw_last_error(sqlite3_db_handle(stmt));
    }
}

void bind_null(sqlite3_stmt* stmt, int index) {
    if (sqlite3_bind_null(stmt, index) != SQLITE_OK) {
        throw_last_error(sqlite3_db_handle(stmt));
    }
}

bool column_is_null(sqlite3_stmt* stmt, int index) noexcept {
    return sqlite3_column_type(stmt, index) == SQLITE_NULL;
}

std::int64_t column_int64(sqlite3_stmt* stmt, int index) noexcept {
    return sqlite3_column_int64(stmt, index);
}

double column_double(sqlite3_stmt* stmt, int index) noexcept {
    return sqlite3_column_double(stmt, index);
}

std::string column_text(sqlite3_stmt* stmt, int index) {
    const auto* text = sqlite3_column_text(stmt, index);
    if (text == nullptr) {
        return {};
    }
    return std::string(reinterpret_cast<const char*>(text),
                       static_cast<std::size_t>(sqlite3_column_bytes(stmt, index)));
}

void throw_null_column() {
    throw DbError(SQLITE_MISMATCH, "NULL read into a non-optional column");
}

}  // namespace detail

// ---- Statement ------------------------------------------------------------------

Statement::Statement(sqlite3_stmt* stmt) noexcept : stmt_(stmt) {}

Statement::Statement(Statement&& other) noexcept : stmt_(std::exchange(other.stmt_, nullptr)) {}

Statement& Statement::operator=(Statement&& other) noexcept {
    if (this != &other) {
        sqlite3_finalize(stmt_);
        stmt_ = std::exchange(other.stmt_, nullptr);
    }
    return *this;
}

Statement::~Statement() { sqlite3_finalize(stmt_); }

bool Statement::step() {
    if (stmt_ == nullptr) {
        return false;  // empty SQL text: nothing to run
    }
    const int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) {
        return true;
    }
    if (rc == SQLITE_DONE) {
        return false;
    }
    throw_last_error(sqlite3_db_handle(stmt_));
}

void Statement::reset() noexcept { sqlite3_reset(stmt_); }

void Statement::clear_bindings() noexcept { sqlite3_clear_bindings(stmt_); }

int Statement::column_count() const noexcept { return sqlite3_column_count(stmt_); }

sqlite3_stmt* Statement::handle() const noexcept { return stmt_; }

// ---- Database -------------------------------------------------------------------

Database::Database(const std::string& path) {
    sqlite3* db = nullptr;
    const int rc = sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                                   nullptr);
    if (rc != SQLITE_OK) {
        const std::string message = db != nullptr ? sqlite3_errmsg(db) : sqlite3_errstr(rc);
        const int extended = db != nullptr ? sqlite3_extended_errcode(db) : rc;
        sqlite3_close_v2(db);  // safe on null; the handle must be closed even on failure
        throw DbError(extended, message);
    }
    db_ = db;
    sqlite3_extended_result_codes(db_, 1);
    try {
        exec("PRAGMA foreign_keys = ON");
    } catch (...) {
        sqlite3_close_v2(db_);  // the destructor does not run for a failed constructor
        db_ = nullptr;
        throw;
    }
}

Database::Database(Database&& other) noexcept : db_(std::exchange(other.db_, nullptr)) {}

Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
        sqlite3_close_v2(db_);
        db_ = std::exchange(other.db_, nullptr);
    }
    return *this;
}

Database::~Database() { sqlite3_close_v2(db_); }

void Database::exec(std::string_view sql) {
    const std::string text(sql);  // sqlite3_exec needs a NUL-terminated string
    char* error = nullptr;
    const int rc = sqlite3_exec(db_, text.c_str(), nullptr, nullptr, &error);
    if (rc != SQLITE_OK) {
        const std::string message = sqlite3_errmsg(db_);
        const int extended = sqlite3_extended_errcode(db_);
        sqlite3_free(error);
        throw DbError(extended, message);
    }
    sqlite3_free(error);
}

std::expected<void, SqlError> Database::try_exec(std::string_view sql) {
    try {
        exec(sql);
        return {};
    } catch (const DbError& e) {
        return std::unexpected(SqlError{e.primary_code(), e.extended_code(), e.what()});
    }
}

Statement Database::prepare(std::string_view sql) {
    sqlite3_stmt* stmt = nullptr;
    const int rc = sqlite3_prepare_v2(db_, sql.data(), static_cast<int>(sql.size()), &stmt,
                                      nullptr);
    if (rc != SQLITE_OK) {
        throw_last_error(db_);
    }
    return Statement(stmt);
}

std::expected<Statement, SqlError> Database::try_prepare(std::string_view sql) {
    try {
        return prepare(sql);
    } catch (const DbError& e) {
        return std::unexpected(SqlError{e.primary_code(), e.extended_code(), e.what()});
    }
}

std::int64_t Database::last_insert_rowid() const noexcept {
    return sqlite3_last_insert_rowid(db_);
}

int Database::changes() const noexcept { return sqlite3_changes(db_); }

bool Database::in_transaction() const noexcept { return sqlite3_get_autocommit(db_) == 0; }

sqlite3* Database::handle() const noexcept { return db_; }

// ---- Transactions ---------------------------------------------------------------

Transaction::Transaction(Database& db) : db_(&db) { db.exec("BEGIN IMMEDIATE"); }

void Transaction::commit() {
    if (db_ == nullptr) {
        return;  // already committed
    }
    db_->exec("COMMIT");
    db_ = nullptr;
}

Transaction::~Transaction() {
    if (db_ != nullptr && db_->in_transaction()) {
        try {
            db_->exec("ROLLBACK");
        } catch (...) {
            // A destructor must not throw; the connection is in a state we cannot repair here.
        }
    }
}

Savepoint::Savepoint(Database& db, std::string_view name)
    : db_(&db), name_(checked_savepoint_name(name)) {
    db.exec("SAVEPOINT " + name_);
}

void Savepoint::release() {
    if (db_ == nullptr) {
        return;  // already released
    }
    db_->exec("RELEASE " + name_);
    db_ = nullptr;
}

Savepoint::~Savepoint() {
    if (db_ == nullptr) {
        return;
    }
    try {
        db_->exec("ROLLBACK TO " + name_);
        db_->exec("RELEASE " + name_);
    } catch (...) {
        // A destructor must not throw.
    }
}

}  // namespace cppref::database_sqlite
