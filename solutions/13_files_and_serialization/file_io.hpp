// Whole-file I/O helpers: binary read and crash-safe replacement.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace cppref::files {

// Reads the whole file in binary mode. Missing or unreadable file ->
// std::filesystem::filesystem_error.
[[nodiscard]] std::string read_file_binary(const std::filesystem::path& path);

// Replaces `path` with `data` so that readers observe either the old or the new complete
// contents, never a partial file.
// Writes to a uniquely named temporary in the same directory, then renames it over `path`.
// The temporary is removed on any failure. Missing parent directory or `path` being a
// directory -> std::filesystem::filesystem_error.
// Durability note: no fsync is issued (not portable across the supported platforms), so
// the guarantee is against torn files on process crash, not against power loss.
void write_file_atomic(const std::filesystem::path& path, std::string_view data);

}  // namespace cppref::files
