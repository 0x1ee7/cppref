// Part 3 · Build metadata: version parsing, git SHA validation, stamped build info.
#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace cppref::deployment {

/// Strict MAJOR.MINOR.PATCH (no "v", no pre-release, no leading zeros).
struct Version {
    unsigned major = 0;
    unsigned minor = 0;
    unsigned patch = 0;

    auto operator<=>(const Version&) const = default;

    [[nodiscard]] std::string to_string() const;
};

/// Parses "X.Y.Z". Error text: "expected MAJOR.MINOR.PATCH, got '<input>'".
[[nodiscard]] std::expected<Version, std::string> parse_version(std::string_view text);

/// True for 7..40 characters of lowercase hex.
[[nodiscard]] bool is_git_sha(std::string_view text);

/// What the binary was built from. Values come from compile definitions
/// CPPREF_BUILD_VERSION and CPPREF_BUILD_GIT_SHA set by CMake.
struct BuildInfo {
    std::string version;   // e.g. "0.1.0"
    std::string git_sha;   // 7..40 lowercase hex, or "unknown"

    /// "cppref-service <version> (<git_sha>)"
    [[nodiscard]] std::string version_line() const;
};

[[nodiscard]] BuildInfo build_info();

}  // namespace cppref::deployment
