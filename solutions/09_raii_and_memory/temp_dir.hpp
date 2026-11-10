#pragma once

#include <filesystem>
#include <string_view>
#include <utility>

namespace cppref::raii_and_memory {

// Owns a uniquely named directory under temp_directory_path(); removes the tree on destruction.
class TempDir {
public:
    explicit TempDir(std::string_view prefix);

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    TempDir(TempDir&& other) noexcept : path_(std::exchange(other.path_, std::filesystem::path{})) {}

    TempDir& operator=(TempDir&& other) noexcept {
        if (this != &other) {
            remove_owned();
            path_ = std::exchange(other.path_, std::filesystem::path{});
        }
        return *this;
    }

    ~TempDir() { remove_owned(); }

    const std::filesystem::path& path() const noexcept { return path_; }

    // Throws std::logic_error when this object no longer owns a directory.
    std::filesystem::path file(std::string_view name) const;

    // Disowns the directory; it will not be removed by this object.
    std::filesystem::path keep() noexcept { return std::exchange(path_, std::filesystem::path{}); }

private:
    void remove_owned() noexcept {
        if (path_.empty()) return;
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    std::filesystem::path path_;
};

}  // namespace cppref::raii_and_memory
