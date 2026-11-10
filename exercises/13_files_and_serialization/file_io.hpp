// Binary-safe file reading and atomic replacement of whole files.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace cppref::files {

// Reads the whole file as raw bytes (binary mode, no newline translation).
//   missing or unreadable -> std::filesystem::filesystem_error
[[nodiscard]] std::string read_file_binary(const std::filesystem::path& path);

// Replaces `path` with `data` atomically with respect to readers of `path`:
// stage into a uniquely named temporary file in the SAME directory, flush and close it,
// check the stream state, then rename it over `path`.
//   - On any failure the temporary file is removed, the exception propagates, and an
//     existing `path` keeps its old contents.
//   - Errors -> std::filesystem::filesystem_error (e.g. missing parent directory, `path`
//     is an existing directory). Nothing is created when the parent directory is missing.
// Durability: data is flushed to the OS, not fsync'd; std has no portable fsync.
void write_file_atomic(const std::filesystem::path& path, std::string_view data);

}  // namespace cppref::files
