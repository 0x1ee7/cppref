// Tree scanning: a manifest of regular files (size, mtime, SHA-256) and the
// ignore rules that filter it.
#pragma once

#include "timestamp.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace cppref::files {

struct FileEntry {
    std::uint64_t size = 0;
    Timestamp mtime{};    // from the file's last-write time, floored to microseconds
    std::string sha256;   // lowercase hex
    friend bool operator==(const FileEntry&, const FileEntry&) = default;
};

// Keys are relative POSIX paths ("pkg/mod.cpp"). std::map iterates them in sorted
// (bytewise) order; std::less<> allows find(std::string_view) without allocating.
using Manifest = std::map<std::string, FileEntry, std::less<>>;

// Segment-aware glob. Pattern and path are both split on '/':
//   '*'  matches any run of characters within one segment (never '/')
//   '?'  matches exactly one character within one segment
//   '**' as a whole segment matches zero or more segments
//   everything else matches literally (no brackets, no character classes)
[[nodiscard]] bool glob_match(std::string_view pattern, std::string_view path);

// Ignore rules applied to a relative POSIX path:
//   - a pattern WITHOUT '/' matches if it matches ANY path component (file or
//     directory name, at any depth): "*.tmp" drops "a.tmp" and "x/y/b.tmp";
//   - a pattern WITH '/' must match the whole relative path (anchored at the root):
//     "logs/**" drops "logs/a.log" but keeps "other/logs/c.log".
[[nodiscard]] bool is_ignored(std::string_view relative_posix, const std::vector<std::string>& ignore);

// Recursively collects regular files under `root` and hashes each with sha256_file.
// Symlinks (to files or to directories) are skipped and never followed. Files whose
// relative path is ignored are omitted. Directories matched by a no-slash pattern may be
// pruned during the walk; the result is identical to filtering afterwards.
//   root does not exist  -> std::filesystem::filesystem_error (no_such_file_or_directory)
//   root is not a dir    -> std::filesystem::filesystem_error (not_a_directory)
[[nodiscard]] Manifest scan_tree(const std::filesystem::path& root,
                                 const std::vector<std::string>& ignore = {});

}  // namespace cppref::files
