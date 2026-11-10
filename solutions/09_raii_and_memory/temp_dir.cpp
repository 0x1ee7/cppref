#include "temp_dir.hpp"

#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

namespace cppref::raii_and_memory {

namespace {

std::uint64_t next_random() {
    static thread_local std::mt19937_64 gen{std::random_device{}()};
    return std::uniform_int_distribution<std::uint64_t>{}(gen);
}

}  // namespace

TempDir::TempDir(std::string_view prefix) {
    const fs::path base = fs::temp_directory_path();
    // create_directory returns false without error when the name exists: retry with a new suffix.
    for (int attempt = 0; attempt < 100; ++attempt) {
        const fs::path candidate = base / (std::string(prefix) + "-" + std::to_string(next_random()));
        std::error_code ec;
        if (fs::create_directory(candidate, ec)) {
            path_ = candidate;
            return;
        }
        if (ec) throw fs::filesystem_error("TempDir: create_directory failed", candidate, ec);
    }
    throw fs::filesystem_error("TempDir: no unique name found", base,
                               std::make_error_code(std::errc::file_exists));
}

fs::path TempDir::file(std::string_view name) const {
    if (path_.empty()) throw std::logic_error("TempDir owns no directory");
    return path_ / name;
}

}  // namespace cppref::raii_and_memory
