#pragma once

#include <filesystem>
#include <string_view>

namespace cppref::raii_and_memory {

// Owns a fresh directory under std::filesystem::temp_directory_path() and removes the whole tree
// when destroyed. Move-only.
class TempDir {
public:
    // Creates an empty, uniquely named directory whose name starts with prefix.
    // Throws std::filesystem::filesystem_error when creation fails.
    explicit TempDir(std::string_view prefix);

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    // The source is left owning nothing (path() is empty).
    TempDir(TempDir&& other) noexcept { (void)other; }
    // Removes the directory this object owns, then takes other's. Self-move is a no-op.
    TempDir& operator=(TempDir&& other) noexcept {
        (void)other;
        return *this;
    }

    // Removes the tree. Errors are ignored; never throws.
    ~TempDir() {}

    // The owned directory, or empty when owning nothing.
    const std::filesystem::path& path() const noexcept { return path_; }

    // path() / name. Throws std::logic_error when owning nothing.
    std::filesystem::path file(std::string_view name) const;

    // Disowns the directory: returns its path and the destructor will no longer remove it.
    // Afterwards path() is empty.
    std::filesystem::path keep() noexcept { return {}; }

private:
    std::filesystem::path path_;
};

}  // namespace cppref::raii_and_memory
