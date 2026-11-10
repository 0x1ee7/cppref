#include "manifest_store.hpp"

#include "file_io.hpp"
#include "json.hpp"
#include "sync.hpp"

#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cppref::files {

namespace {

bool is_lower_hex_digest(std::string_view s) {
    if (s.size() != 64) {
        return false;
    }
    for (const char c : s) {
        const bool digit = c >= '0' && c <= '9';
        const bool lower = c >= 'a' && c <= 'f';
        if (!digit && !lower) {
            return false;
        }
    }
    return true;
}

const Json& field(const Json& object, std::string_view key) {
    const Json* value = object.find(key);
    if (value == nullptr) {
        throw JsonError("missing field \"" + std::string(key) + "\"");
    }
    return *value;
}

}  // namespace

std::string manifest_to_json(const Manifest& manifest) {
    Json::Object entries;
    entries.reserve(manifest.size());
    for (const auto& [key, entry] : manifest) {
        validate_relative_key(key);
        if (!is_lower_hex_digest(entry.sha256)) {
            throw std::invalid_argument("invalid sha256 for '" + key + "'");
        }
        if (entry.size > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
            throw std::invalid_argument("size too large for '" + key + "'");
        }
        entries.emplace_back(
            key, Json::object({
                     {"size", Json::integer(static_cast<std::int64_t>(entry.size))},
                     {"mtime_us", Json::integer(entry.mtime.time_since_epoch().count())},
                     {"sha256", Json::string(entry.sha256)},
                 }));
    }
    return Json::object({
                            {"format", Json::integer(manifest_format_version)},
                            {"entries", Json::object(std::move(entries))},
                        })
        .dump(2);
}

Manifest manifest_from_json(std::string_view text) {
    const Json doc = Json::parse(text);
    if (doc.kind() != Json::Kind::object) {
        throw JsonError("manifest must be a JSON object");
    }
    if (field(doc, "format").as_integer() != manifest_format_version) {
        throw JsonError("unsupported manifest format");
    }

    Manifest manifest;
    for (const auto& [key, value] : field(doc, "entries").as_object()) {
        validate_relative_key(key);
        const std::int64_t size = field(value, "size").as_integer();
        if (size < 0) {
            throw JsonError("negative size for '" + key + "'");
        }
        const std::int64_t mtime_us = field(value, "mtime_us").as_integer();
        const std::string& sha = field(value, "sha256").as_string();
        if (!is_lower_hex_digest(sha)) {
            throw JsonError("invalid sha256 for '" + key + "'");
        }
        manifest.emplace(key, FileEntry{
                                  static_cast<std::uint64_t>(size),
                                  Timestamp{std::chrono::microseconds{mtime_us}},
                                  sha,
                              });
    }
    return manifest;
}

void save_manifest_atomic(const Manifest& manifest, const std::filesystem::path& path) {
    std::string text = manifest_to_json(manifest);
    text += '\n';
    write_file_atomic(path, text);
}

Manifest load_manifest(const std::filesystem::path& path) {
    return manifest_from_json(read_file_binary(path));
}

}  // namespace cppref::files
