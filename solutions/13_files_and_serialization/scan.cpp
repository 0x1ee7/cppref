#include "scan.hpp"

#include "digest.hpp"

#include <chrono>
#include <cstddef>
#include <string_view>
#include <system_error>
#include <vector>

namespace cppref::files {
namespace fs = std::filesystem;

namespace {

std::vector<std::string_view> split_slash(std::string_view s) {
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    for (;;) {
        const auto pos = s.find('/', start);
        if (pos == std::string_view::npos) {
            parts.push_back(s.substr(start));
            return parts;
        }
        parts.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }
}

// Wildcard match within one segment; '*' and '?' never see across '/' because the
// caller has already split on it.
bool match_segment(std::string_view p, std::string_view t) {
    constexpr std::size_t none = std::string_view::npos;
    std::size_t pi = 0, ti = 0, star = none, mark = 0;
    while (ti < t.size()) {
        if (pi < p.size() && p[pi] == '*') {
            star = pi++;
            mark = ti;
        } else if (pi < p.size() && (p[pi] == '?' || p[pi] == t[ti])) {
            ++pi;
            ++ti;
        } else if (star != none) {
            pi = star + 1;
            ti = ++mark;
        } else {
            return false;
        }
    }
    while (pi < p.size() && p[pi] == '*') {
        ++pi;
    }
    return pi == p.size();
}

bool match_parts(const std::vector<std::string_view>& pat, std::size_t pi,
                 const std::vector<std::string_view>& path, std::size_t si) {
    if (pi == pat.size()) {
        return si == path.size();
    }
    if (pat[pi] == "**") {
        for (std::size_t k = si; k <= path.size(); ++k) {
            if (match_parts(pat, pi + 1, path, k)) {
                return true;
            }
        }
        return false;
    }
    if (si == path.size()) {
        return false;
    }
    return match_segment(pat[pi], path[si]) && match_parts(pat, pi + 1, path, si + 1);
}

Timestamp file_mtime(const fs::path& p) {
    const auto file_time = fs::last_write_time(p);
    const auto sys_time = std::chrono::clock_cast<std::chrono::system_clock>(file_time);
    return std::chrono::floor<std::chrono::microseconds>(sys_time);
}

}  // namespace

bool glob_match(std::string_view pattern, std::string_view path) {
    return match_parts(split_slash(pattern), 0, split_slash(path), 0);
}

bool is_ignored(std::string_view relative_posix, const std::vector<std::string>& ignore) {
    const auto components = split_slash(relative_posix);
    for (const auto& pattern : ignore) {
        if (pattern.find('/') == std::string::npos) {
            for (const auto component : components) {
                if (glob_match(pattern, component)) {
                    return true;
                }
            }
        } else if (glob_match(pattern, relative_posix)) {
            return true;
        }
    }
    return false;
}

Manifest scan_tree(const fs::path& root, const std::vector<std::string>& ignore) {
    const fs::file_status st = fs::status(root);
    if (!fs::exists(st)) {
        throw fs::filesystem_error("scan root does not exist", root,
                                   std::make_error_code(std::errc::no_such_file_or_directory));
    }
    if (!fs::is_directory(st)) {
        throw fs::filesystem_error("scan root is not a directory", root,
                                   std::make_error_code(std::errc::not_a_directory));
    }

    Manifest manifest;
    // Default options: symlinks are not followed.
    for (fs::recursive_directory_iterator it(root), end; it != end; ++it) {
        const fs::directory_entry& entry = *it;
        if (entry.is_symlink() || !entry.is_regular_file()) {
            continue;
        }
        std::string key = fs::relative(entry.path(), root).generic_string();
        if (is_ignored(key, ignore)) {
            continue;
        }
        FileEntry file{
            static_cast<std::uint64_t>(entry.file_size()),
            file_mtime(entry.path()),
            sha256_file(entry.path()),
        };
        manifest.emplace(std::move(key), std::move(file));
    }
    return manifest;
}

}  // namespace cppref::files
