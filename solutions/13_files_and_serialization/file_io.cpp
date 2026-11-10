#include "file_io.hpp"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <random>
#include <string_view>
#include <system_error>
#include <utility>

namespace cppref::files {
namespace fs = std::filesystem;

namespace {

// Removes the temporary file unless commit() was called.
class TempFileGuard {
public:
    explicit TempFileGuard(fs::path path) : path_(std::move(path)) {}
    ~TempFileGuard() {
        if (!committed_) {
            std::error_code ec;
            fs::remove(path_, ec);
        }
    }
    TempFileGuard(const TempFileGuard&) = delete;
    TempFileGuard& operator=(const TempFileGuard&) = delete;

    void commit() noexcept { committed_ = true; }

private:
    fs::path path_;
    bool committed_ = false;
};

fs::path unique_sibling(const fs::path& target) {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    for (;;) {
        char hex[17];
        std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(rng()));
        fs::path candidate = target.parent_path() / (target.filename().string() + ".tmp-" + hex);
        if (!fs::exists(candidate)) {
            return candidate;
        }
    }
}

}  // namespace

std::string read_file_binary(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        const bool exists = fs::exists(path);
        throw fs::filesystem_error(
            "cannot open file", path,
            std::make_error_code(exists ? std::errc::permission_denied : std::errc::no_such_file_or_directory));
    }
    std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (in.bad()) {
        throw fs::filesystem_error("read failed", path, std::make_error_code(std::errc::io_error));
    }
    return data;
}

void write_file_atomic(const fs::path& path, std::string_view data) {
    const fs::path dir = path.parent_path().empty() ? fs::path(".") : path.parent_path();
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        throw fs::filesystem_error("parent directory does not exist", path,
                                   std::make_error_code(std::errc::no_such_file_or_directory));
    }
    if (fs::is_directory(path, ec)) {
        throw fs::filesystem_error("target is a directory", path,
                                   std::make_error_code(std::errc::is_a_directory));
    }

    const fs::path temp = unique_sibling(path);
    TempFileGuard guard(temp);
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            throw fs::filesystem_error("cannot create temporary file", temp,
                                       std::make_error_code(std::errc::permission_denied));
        }
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
        out.close();
        if (out.fail()) {
            throw fs::filesystem_error("failed to write temporary file", temp,
                                       std::make_error_code(std::errc::io_error));
        }
    }
    fs::rename(temp, path);
    guard.commit();
}

}  // namespace cppref::files
