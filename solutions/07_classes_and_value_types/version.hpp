// Part 8: Version — immutable semantic version with precedence and build metadata.
#pragma once

#include <compare>
#include <cstddef>
#include <expected>
#include <functional>
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
    [[nodiscard]] static std::expected<Version, VersionError> create(int major, int minor = 0, int patch = 0,
                                                                     std::string_view prerelease = {},
                                                                     std::string_view build = {});

    [[nodiscard]] static std::expected<Version, VersionError> parse(std::string_view text);

    [[nodiscard]] int major() const;
    [[nodiscard]] int minor() const;
    [[nodiscard]] int patch() const;
    [[nodiscard]] std::string_view prerelease() const;
    [[nodiscard]] std::string_view build() const;

    [[nodiscard]] std::expected<Version, VersionError> bump(BumpPart part) const;

    [[nodiscard]] std::string to_string() const;

    friend bool operator==(const Version& lhs, const Version& rhs);
    friend std::strong_ordering operator<=>(const Version& lhs, const Version& rhs);
    friend std::ostream& operator<<(std::ostream& out, const Version& version);

    friend struct std::hash<Version>;

private:
    Version(int major, int minor, int patch, std::string prerelease, std::string build)
        : major_(major), minor_(minor), patch_(patch), prerelease_(std::move(prerelease)), build_(std::move(build)) {}

    int major_;
    int minor_;
    int patch_;
    std::string prerelease_;
    std::string build_;
};

}  // namespace cppref::classes

template <>
struct std::hash<cppref::classes::Version> {
    std::size_t operator()(const cppref::classes::Version& version) const;
};
