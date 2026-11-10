// Part 8: Version — immutable semantic version with precedence and build metadata.
#pragma once

#include <compare>
#include <cstddef>
#include <expected>
#include <iosfwd>
#include <string>
#include <string_view>

namespace cppref::classes {

enum class VersionError {
    invalid_component,   ///< major/minor/patch negative, or not a number, or leading zeros
    invalid_prerelease,  ///< bad identifier, or numeric identifier with leading zeros
    invalid_build,       ///< bad identifier (build identifiers may have leading zeros)
    invalid_syntax,      ///< parse(): text is not [v]MAJOR.MINOR.PATCH[-PRE][+BUILD]
    overflow,            ///< bump() would exceed INT_MAX
};

enum class BumpPart { major, minor, patch };

/// Semantic version 2.0 value type. Build metadata is ignored for ==, ordering and hashing.
class Version {
public:
    /// prerelease/build: dot-separated identifiers of [0-9A-Za-z-]+; empty means none.
    [[nodiscard]] static std::expected<Version, VersionError> create(int major, int minor = 0, int patch = 0,
                                                                     std::string_view prerelease = {},
                                                                     std::string_view build = {});

    /// Strict: [v]MAJOR.MINOR.PATCH[-PRERELEASE][+BUILD]. Core numbers have no leading zeros.
    [[nodiscard]] static std::expected<Version, VersionError> parse(std::string_view text);

    [[nodiscard]] int major() const;
    [[nodiscard]] int minor() const;
    [[nodiscard]] int patch() const;
    [[nodiscard]] std::string_view prerelease() const;  ///< empty when none
    [[nodiscard]] std::string_view build() const;       ///< empty when none

    /// Increment one part, reset lower parts, clear prerelease and build.
    [[nodiscard]] std::expected<Version, VersionError> bump(BumpPart part) const;

    /// Canonical form, e.g. "1.2.3-rc.1+build.5" (build included).
    [[nodiscard]] std::string to_string() const;

    /// Equal cores and prerelease. Build is ignored.
    friend bool operator==(const Version& lhs, const Version& rhs);

    /// SemVer precedence: core numerically; a release outranks any prerelease with the same core;
    /// prerelease identifiers compared left to right (numeric < alphanumeric, shorter prefix is lower).
    /// Build is ignored.
    friend std::strong_ordering operator<=>(const Version& lhs, const Version& rhs);

    /// Writes to_string(), including build.
    friend std::ostream& operator<<(std::ostream& out, const Version& version);

    // engineer: private data members (major_, minor_, patch_, prerelease_, build_)
};

}  // namespace cppref::classes

template <>
struct std::hash<cppref::classes::Version> {
    /// Hashes core and prerelease only, consistent with operator==.
    std::size_t operator()(const cppref::classes::Version& version) const;
};
