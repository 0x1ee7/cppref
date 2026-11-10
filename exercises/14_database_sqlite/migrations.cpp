#include "migrations.hpp"

#include <stdexcept>

namespace cppref::database_sqlite {

MigrationError::MigrationError(int version, int extended_code, const std::string& message)
    : std::runtime_error(message) {
    (void)version;
    (void)extended_code;
    throw std::logic_error("not implemented: MigrationError::MigrationError");
}

// noexcept accessors return placeholders: throwing here would call std::terminate.
int MigrationError::version() const noexcept {
    return 0;
}

int MigrationError::extended_code() const noexcept {
    return 0;
}

std::span<const Migration> builtin_migrations() {
    throw std::logic_error("not implemented: builtin_migrations");
}

int schema_version(Database& db) {
    (void)db;
    throw std::logic_error("not implemented: schema_version");
}

int migrate(Database& db, std::span<const Migration> migrations) {
    (void)db;
    (void)migrations;
    throw std::logic_error("not implemented: migrate");
}

}  // namespace cppref::database_sqlite
