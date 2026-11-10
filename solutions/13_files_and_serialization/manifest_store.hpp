// Manifest persistence as versioned JSON.
#pragma once

#include "scan.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace cppref::files {

inline constexpr int manifest_format_version = 1;

// Serializes to pretty-printed JSON (indent 2):
//   {"format": 1, "entries": {"<key>": {"size": N, "mtime_us": N, "sha256": "<hex>"}}}
// Keys are validated with validate_relative_key (std::invalid_argument on failure).
// sha256 must be 64 lowercase hex characters, else std::invalid_argument.
// size > INT64_MAX -> std::invalid_argument.
[[nodiscard]] std::string manifest_to_json(const Manifest& manifest);

// Strict inverse of manifest_to_json.
//   malformed JSON, wrong types, missing fields, bad hex, negative size,
//   or a format other than manifest_format_version -> JsonError
//   unsafe key -> std::invalid_argument
[[nodiscard]] Manifest manifest_from_json(std::string_view text);

// Writes manifest_to_json(manifest) plus a trailing newline via write_file_atomic.
void save_manifest_atomic(const Manifest& manifest, const std::filesystem::path& path);

// Reads and parses a manifest file. Missing file -> std::filesystem::filesystem_error.
[[nodiscard]] Manifest load_manifest(const std::filesystem::path& path);

}  // namespace cppref::files
