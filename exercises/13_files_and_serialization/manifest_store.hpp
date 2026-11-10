// Manifest persistence: a versioned JSON document, written atomically.
#pragma once

#include "scan.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace cppref::files {

inline constexpr std::int64_t manifest_format_version = 1;

// Document shape (keys sorted by dump):
//   {"format": 1, "entries": {"<key>": {"size": n, "mtime_us": n, "sha256": "<64 lowercase hex>"}}}
// mtime_us is microseconds since the Unix epoch (may be negative).

// Serialises with Json::dump(2). Invalid input is a caller bug and is rejected before any output:
// std::invalid_argument for a key failing validate_relative_key or a sha256 that is not
// 64 lowercase hex characters.
[[nodiscard]] std::string manifest_to_json(const Manifest& manifest);

// Parses and validates the document shape. JsonError for malformed JSON, a wrong or missing
// format/entries/size/mtime_us/sha256 field, a negative size, or a digest that is not 64 lowercase
// hex characters. std::invalid_argument for a key failing validate_relative_key.
[[nodiscard]] Manifest manifest_from_json(std::string_view text);

// manifest_to_json plus a trailing '\n', written with write_file_atomic.
void save_manifest_atomic(const Manifest& manifest, const std::filesystem::path& path);

// Reads and parses a manifest file. Missing file -> std::filesystem::filesystem_error.
[[nodiscard]] Manifest load_manifest(const std::filesystem::path& path);

}  // namespace cppref::files
