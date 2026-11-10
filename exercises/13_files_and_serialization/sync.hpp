// Manifest diff and a traversal-safe apply step for the backup/sync tool.
#pragma once

#include "scan.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace cppref::files {

struct ManifestDiff {
    std::vector<std::string> added;     // only in the new manifest, sorted
    std::vector<std::string> removed;   // only in the old manifest, sorted
    std::vector<std::string> modified;  // in both, size or sha256 differs, sorted
    // An mtime-only change is NOT a modification.

    [[nodiscard]] bool empty() const noexcept;
    friend bool operator==(const ManifestDiff&, const ManifestDiff&) = default;
};

[[nodiscard]] ManifestDiff diff_manifests(const Manifest& old_manifest, const Manifest& new_manifest);

// Accepts only plain relative keys. Throws std::invalid_argument naming the key when it:
//   - is empty;
//   - is absolute: starts with '/' or '\\', or has a drive prefix such as "C:";
//   - has an empty, "." or ".." component.
// '/' and '\\' both separate components on every platform, so "a\\..\\b" is rejected
// on Linux too: a tree synced from Windows must not escape on Linux.
void validate_relative_key(std::string_view key);

struct ApplyResult {
    std::vector<std::string> copied;   // added keys, then modified keys
    std::vector<std::string> deleted;  // removed keys that existed in dest
};

// Every key in added/modified/removed is validated BEFORE any disk change.
//   - added and modified: copied from source/<key> to dest/<key>, creating parent
//     directories and preserving the last-write time (copy2 semantics);
//   - removed: deleted from dest; an already-absent file is not an error and is not
//     reported; afterwards, parent directories left empty are removed, climbing up to but
//     NOT including dest.
// Filesystem failures after validation propagate as std::filesystem::filesystem_error;
// changes made before the failure are not rolled back.
[[nodiscard]] ApplyResult apply_diff(const ManifestDiff& diff,
                                     const std::filesystem::path& source,
                                     const std::filesystem::path& dest);

}  // namespace cppref::files
