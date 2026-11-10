#pragma once

// Shared test helpers (test-only; compiled as *_test.hpp). Every database lives in a
// unique temp file that is removed, with its journal files, when the fixture dies.

#include "migrations.hpp"
#include "model.hpp"
#include "repositories.hpp"
#include "sqlite_wrapper.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace cppref::database_sqlite::testing {

// Unique file path under temp_directory_path(); deletes the file and its -journal, -wal and
// -shm siblings on destruction.
class TempDbFile {
public:
    TempDbFile() : path_(make_path()) {}
    TempDbFile(const TempDbFile&) = delete;
    TempDbFile& operator=(const TempDbFile&) = delete;
    ~TempDbFile() {
        std::error_code ec;
        for (const char* suffix : {"", "-journal", "-wal", "-shm"}) {
            std::filesystem::remove(std::filesystem::path(path_.string() + suffix), ec);
        }
    }
    const std::filesystem::path& path() const noexcept { return path_; }
    std::string string() const { return path_.string(); }

private:
    static std::filesystem::path make_path() {
        static std::atomic<unsigned> counter{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        return std::filesystem::temp_directory_path() /
               ("cppref_sqlite_" + std::to_string(stamp) + "_" + std::to_string(counter++) + ".db");
    }
    std::filesystem::path path_;
};

// A fresh file database. Members are ordered so the connection closes before the file is
// removed. By default the builtin migrations are applied.
class TestDb {
public:
    explicit TestDb(bool with_migrations = true) : db_(file_.string()) {
        if (with_migrations) {
            migrate(db_);
        }
    }
    Database& db() noexcept { return db_; }

private:
    TempDbFile file_;
    Database db_;
};

inline Timestamp at(std::int64_t seconds) {
    return Timestamp{std::chrono::seconds{seconds}};
}

// Catalogue: WIDGET 1000 cents, GADGET 2550 cents, GIZMO 325 cents.
inline void seed_catalogue(Database& db) {
    ProductRepository products(db);
    const std::vector<Product> rows{
        {"WIDGET", "Widget", Money{1000}},
        {"GADGET", "Gadget", Money{2550}},
        {"GIZMO", "Gizmo", Money{325}},
    };
    products.upsert_many(rows);
}

}  // namespace cppref::database_sqlite::testing
