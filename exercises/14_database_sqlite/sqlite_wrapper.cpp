#include "sqlite_wrapper.hpp"

#include <stdexcept>

namespace cppref::database_sqlite {

DbError::DbError(int extended_code, const std::string& message)
    : std::runtime_error(message) {
    (void)extended_code;
    throw std::logic_error("not implemented: DbError::DbError");
}

// noexcept accessors return placeholders: throwing here would call std::terminate.
int DbError::extended_code() const noexcept {
    return 0;
}

int DbError::primary_code() const noexcept {
    return 0;
}

Statement::Statement(sqlite3_stmt* stmt) noexcept : stmt_(stmt) {}

Statement::Statement(Statement&& other) noexcept : stmt_(other.stmt_) {
    other.stmt_ = nullptr;
}

Statement& Statement::operator=(Statement&& other) noexcept {
    stmt_ = other.stmt_;
    other.stmt_ = nullptr;
    return *this;
}

Statement::~Statement() = default;

bool Statement::step() {
    throw std::logic_error("not implemented: Statement::step");
}

void Statement::reset() noexcept {}

void Statement::clear_bindings() noexcept {}

int Statement::column_count() const noexcept {
    return 0;
}

sqlite3_stmt* Statement::handle() const noexcept {
    return stmt_;
}

Database::Database(const std::string& path) {
    (void)path;
    throw std::logic_error("not implemented: Database::Database");
}

Database::Database(Database&& other) noexcept : db_(other.db_) {
    other.db_ = nullptr;
}

Database& Database::operator=(Database&& other) noexcept {
    db_ = other.db_;
    other.db_ = nullptr;
    return *this;
}

Database::~Database() = default;

void Database::exec(std::string_view sql) {
    (void)sql;
    throw std::logic_error("not implemented: Database::exec");
}

std::expected<void, SqlError> Database::try_exec(std::string_view sql) {
    (void)sql;
    throw std::logic_error("not implemented: Database::try_exec");
}

Statement Database::prepare(std::string_view sql) {
    (void)sql;
    throw std::logic_error("not implemented: Database::prepare");
}

std::expected<Statement, SqlError> Database::try_prepare(std::string_view sql) {
    (void)sql;
    throw std::logic_error("not implemented: Database::try_prepare");
}

std::int64_t Database::last_insert_rowid() const noexcept {
    return 0;
}

int Database::changes() const noexcept {
    return 0;
}

bool Database::in_transaction() const noexcept {
    return false;
}

sqlite3* Database::handle() const noexcept {
    return db_;
}

Transaction::Transaction(Database& db) : db_(&db) {
    throw std::logic_error("not implemented: Transaction::Transaction");
}

Transaction::~Transaction() = default;

void Transaction::commit() {
    throw std::logic_error("not implemented: Transaction::commit");
}

Savepoint::Savepoint(Database& db, std::string_view name) : db_(&db), name_(name) {
    throw std::logic_error("not implemented: Savepoint::Savepoint");
}

Savepoint::~Savepoint() = default;

void Savepoint::release() {
    throw std::logic_error("not implemented: Savepoint::release");
}

}  // namespace cppref::database_sqlite
