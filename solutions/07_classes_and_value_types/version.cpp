#include "version.hpp"

#include <array>
#include <limits>
#include <ostream>
#include <utility>
#include <vector>

namespace cppref::classes {
namespace {

constexpr int kMaxComponent = std::numeric_limits<int>::max();

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool is_identifier_char(char c) {
    return is_digit(c) || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '-';
}

bool all_digits(std::string_view text) {
    if (text.empty()) {
        return false;
    }
    for (char c : text) {
        if (!is_digit(c)) {
            return false;
        }
    }
    return true;
}

/// Dot-separated identifiers. Numeric identifiers may not have leading zeros when `strict`.
bool valid_identifiers(std::string_view text, bool strict) {
    std::size_t start = 0;
    while (true) {
        const auto dot = text.find('.', start);
        const auto id = text.substr(start, dot == std::string_view::npos ? std::string_view::npos : dot - start);
        if (id.empty()) {
            return false;
        }
        for (char c : id) {
            if (!is_identifier_char(c)) {
                return false;
            }
        }
        if (strict && all_digits(id) && id.size() > 1 && id.front() == '0') {
            return false;
        }
        if (dot == std::string_view::npos) {
            return true;
        }
        start = dot + 1;
    }
}

std::expected<int, VersionError> parse_component(std::string_view text) {
    if (!all_digits(text) || (text.size() > 1 && text.front() == '0')) {
        return std::unexpected(VersionError::invalid_syntax);
    }
    long long value = 0;
    for (char c : text) {
        value = value * 10 + (c - '0');
        if (value > kMaxComponent) {
            return std::unexpected(VersionError::overflow);
        }
    }
    return static_cast<int>(value);
}

std::vector<std::string_view> split_identifiers(std::string_view text) {
    std::vector<std::string_view> ids;
    std::size_t start = 0;
    while (true) {
        const auto dot = text.find('.', start);
        ids.push_back(text.substr(start, dot == std::string_view::npos ? std::string_view::npos : dot - start));
        if (dot == std::string_view::npos) {
            return ids;
        }
        start = dot + 1;
    }
}

std::strong_ordering compare_identifier(std::string_view a, std::string_view b) {
    const bool a_numeric = all_digits(a);
    const bool b_numeric = all_digits(b);
    if (a_numeric && b_numeric) {
        if (a.size() != b.size()) {
            return a.size() <=> b.size();
        }
        return a.compare(b) <=> 0;
    }
    if (a_numeric != b_numeric) {
        return a_numeric ? std::strong_ordering::less : std::strong_ordering::greater;
    }
    return a.compare(b) <=> 0;
}

std::strong_ordering compare_prerelease(std::string_view a, std::string_view b) {
    const auto lhs = split_identifiers(a);
    const auto rhs = split_identifiers(b);
    const std::size_t common = std::min(lhs.size(), rhs.size());
    for (std::size_t i = 0; i < common; ++i) {
        if (auto order = compare_identifier(lhs[i], rhs[i]); order != 0) {
            return order;
        }
    }
    return lhs.size() <=> rhs.size();
}

}  // namespace

std::expected<Version, VersionError> Version::create(int major, int minor, int patch, std::string_view prerelease,
                                                     std::string_view build) {
    if (major < 0 || minor < 0 || patch < 0) {
        return std::unexpected(VersionError::invalid_component);
    }
    if (!prerelease.empty() && !valid_identifiers(prerelease, true)) {
        return std::unexpected(VersionError::invalid_prerelease);
    }
    if (!build.empty() && !valid_identifiers(build, false)) {
        return std::unexpected(VersionError::invalid_build);
    }
    return Version(major, minor, patch, std::string(prerelease), std::string(build));
}

std::expected<Version, VersionError> Version::parse(std::string_view text) {
    std::string_view rest = text;
    if (!rest.empty() && rest.front() == 'v') {
        rest.remove_prefix(1);
    }

    std::string_view build;
    if (const auto plus = rest.find('+'); plus != std::string_view::npos) {
        build = rest.substr(plus + 1);
        rest = rest.substr(0, plus);
        if (build.empty()) {
            return std::unexpected(VersionError::invalid_syntax);
        }
    }

    std::string_view prerelease;
    if (const auto dash = rest.find('-'); dash != std::string_view::npos) {
        prerelease = rest.substr(dash + 1);
        rest = rest.substr(0, dash);
        if (prerelease.empty()) {
            return std::unexpected(VersionError::invalid_syntax);
        }
    }

    const auto first_dot = rest.find('.');
    if (first_dot == std::string_view::npos) {
        return std::unexpected(VersionError::invalid_syntax);
    }
    const auto second_dot = rest.find('.', first_dot + 1);
    if (second_dot == std::string_view::npos || rest.find('.', second_dot + 1) != std::string_view::npos) {
        return std::unexpected(VersionError::invalid_syntax);
    }

    auto major = parse_component(rest.substr(0, first_dot));
    if (!major) {
        return std::unexpected(major.error());
    }
    auto minor = parse_component(rest.substr(first_dot + 1, second_dot - first_dot - 1));
    if (!minor) {
        return std::unexpected(minor.error());
    }
    auto patch = parse_component(rest.substr(second_dot + 1));
    if (!patch) {
        return std::unexpected(patch.error());
    }
    return create(*major, *minor, *patch, prerelease, build);
}

int Version::major() const {
    return major_;
}

int Version::minor() const {
    return minor_;
}

int Version::patch() const {
    return patch_;
}

std::string_view Version::prerelease() const {
    return prerelease_;
}

std::string_view Version::build() const {
    return build_;
}

std::expected<Version, VersionError> Version::bump(BumpPart part) const {
    if (part == BumpPart::major) {
        if (major_ == kMaxComponent) {
            return std::unexpected(VersionError::overflow);
        }
        return Version(major_ + 1, 0, 0, {}, {});
    }
    if (part == BumpPart::minor) {
        if (minor_ == kMaxComponent) {
            return std::unexpected(VersionError::overflow);
        }
        return Version(major_, minor_ + 1, 0, {}, {});
    }
    if (patch_ == kMaxComponent) {
        return std::unexpected(VersionError::overflow);
    }
    return Version(major_, minor_, patch_ + 1, {}, {});
}

std::string Version::to_string() const {
    std::string out = std::to_string(major_) + '.' + std::to_string(minor_) + '.' + std::to_string(patch_);
    if (!prerelease_.empty()) {
        out += '-';
        out += prerelease_;
    }
    if (!build_.empty()) {
        out += '+';
        out += build_;
    }
    return out;
}

bool operator==(const Version& lhs, const Version& rhs) {
    return lhs.major_ == rhs.major_ && lhs.minor_ == rhs.minor_ && lhs.patch_ == rhs.patch_ &&
           lhs.prerelease_ == rhs.prerelease_;
}

std::strong_ordering operator<=>(const Version& lhs, const Version& rhs) {
    if (auto order = lhs.major_ <=> rhs.major_; order != 0) {
        return order;
    }
    if (auto order = lhs.minor_ <=> rhs.minor_; order != 0) {
        return order;
    }
    if (auto order = lhs.patch_ <=> rhs.patch_; order != 0) {
        return order;
    }
    const bool lhs_pre = !lhs.prerelease_.empty();
    const bool rhs_pre = !rhs.prerelease_.empty();
    if (lhs_pre != rhs_pre) {
        // A release outranks any prerelease with the same core.
        return lhs_pre ? std::strong_ordering::less : std::strong_ordering::greater;
    }
    if (!lhs_pre) {
        return std::strong_ordering::equal;
    }
    return compare_prerelease(lhs.prerelease_, rhs.prerelease_);
}

std::ostream& operator<<(std::ostream& out, const Version& version) {
    return out << version.to_string();
}

}  // namespace cppref::classes

std::size_t std::hash<cppref::classes::Version>::operator()(const cppref::classes::Version& version) const {
    std::size_t seed = std::hash<int>{}(version.major_);
    const auto mix = [&seed](std::size_t value) { seed ^= value + 0x9e3779b9u + (seed << 6) + (seed >> 2); };
    mix(std::hash<int>{}(version.minor_));
    mix(std::hash<int>{}(version.patch_));
    mix(std::hash<std::string>{}(version.prerelease_));
    return seed;
}
