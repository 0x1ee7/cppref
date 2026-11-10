#include "sync.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cppref::files {
namespace fs = std::filesystem;

namespace {

// Converts a validated key into a platform path, treating both separators alike.
fs::path to_path(std::string_view key) {
    fs::path out;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= key.size(); ++i) {
        if (i == key.size() || key[i] == '/' || key[i] == '\\') {
            out /= fs::path(std::string(key.substr(start, i - start)));
            start = i + 1;
        }
    }
    return out;
}

}  // namespace

bool ManifestDiff::empty() const noexcept {
    return added.empty() && removed.empty() && modified.empty();
}

ManifestDiff diff_manifests(const Manifest& old_manifest, const Manifest& new_manifest) {
    ManifestDiff diff;
    for (const auto& [key, fresh] : new_manifest) {
        const auto it = old_manifest.find(key);
        if (it == old_manifest.end()) {
            diff.added.push_back(key);
        } else if (it->second.size != fresh.size || it->second.sha256 != fresh.sha256) {
            diff.modified.push_back(key);
        }
    }
    for (const auto& old_entry : old_manifest) {
        if (!new_manifest.contains(old_entry.first)) {
            diff.removed.push_back(old_entry.first);
        }
    }
    return diff;
}

void validate_relative_key(std::string_view key) {
    const auto reject = [&](const char* why) {
        throw std::invalid_argument(std::string(why) + ": '" + std::string(key) + "'");
    };
    if (key.empty()) {
        reject("empty key");
    }
    if (key.front() == '/' || key.front() == '\\') {
        reject("absolute key");
    }
    const bool ascii_letter = (key.front() >= 'A' && key.front() <= 'Z') ||
                              (key.front() >= 'a' && key.front() <= 'z');
    if (key.size() >= 2 && ascii_letter && key[1] == ':') {
        reject("drive-prefixed key");
    }
    std::size_t start = 0;
    for (std::size_t i = 0; i <= key.size(); ++i) {
        if (i == key.size() || key[i] == '/' || key[i] == '\\') {
            const std::string_view part = key.substr(start, i - start);
            if (part.empty()) {
                reject("empty path component");
            }
            if (part == "." || part == "..") {
                reject("dot path component");
            }
            start = i + 1;
        }
    }
}

ApplyResult apply_diff(const ManifestDiff& diff, const fs::path& source, const fs::path& dest) {
    for (const auto* list : {&diff.added, &diff.modified, &diff.removed}) {
        for (const auto& key : *list) {
            validate_relative_key(key);
        }
    }

    ApplyResult result;
    for (const auto* list : {&diff.added, &diff.modified}) {
        for (const auto& key : *list) {
            const fs::path from = source / to_path(key);
            const fs::path to = dest / to_path(key);
            fs::create_directories(to.parent_path());
            fs::copy_file(from, to, fs::copy_options::overwrite_existing);
            fs::last_write_time(to, fs::last_write_time(from));
            result.copied.push_back(key);
        }
    }

    const fs::path root = dest.lexically_normal();
    for (const auto& key : diff.removed) {
        const fs::path target = root / to_path(key);
        if (!fs::exists(target)) {
            continue;
        }
        fs::remove(target);
        result.deleted.push_back(key);
        for (fs::path parent = target.parent_path();
             parent != root && fs::is_directory(parent) && fs::is_empty(parent);
             parent = parent.parent_path()) {
            fs::remove(parent);
        }
    }
    return result;
}

}  // namespace cppref::files
