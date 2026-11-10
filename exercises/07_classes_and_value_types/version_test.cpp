// Part 8: Version — semantic version precedence, parsing, bumping and value semantics.
#include <gtest/gtest.h>

#include <climits>
#include <sstream>
#include <unordered_set>
#include <vector>

#include "version.hpp"

namespace cc = cppref::classes;

namespace {

cc::Version version(std::string_view text) {
    return cc::Version::parse(text).value();
}

template <typename T, typename E>
void expect_error(const std::expected<T, E>& result, E expected) {
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), expected);
}

}  // namespace

TEST(Part8Version, ParseAcceptsOptionalVPrefix) {
    auto plain = version("1.2.3");
    EXPECT_EQ(plain.major(), 1);
    EXPECT_EQ(plain.minor(), 2);
    EXPECT_EQ(plain.patch(), 3);

    auto prefixed = version("v10.0.1");
    EXPECT_EQ(prefixed.major(), 10);
    EXPECT_EQ(prefixed.to_string(), "10.0.1");
}

TEST(Part8Version, ParseRejectsMalformedText) {
    for (std::string_view bad : {"", "1.2", "1.2.3.4", "a.b.c", "01.2.3", "1.02.3"}) {
        EXPECT_FALSE(cc::Version::parse(bad).has_value()) << bad;
    }
}

TEST(Part8Version, CreateValidatesComponentsAndIdentifiers) {
    expect_error(cc::Version::create(-1), cc::VersionError::invalid_component);
    expect_error(cc::Version::create(1, 2, 3, "01"), cc::VersionError::invalid_prerelease);
    expect_error(cc::Version::create(1, 0, 0, "rc..1"), cc::VersionError::invalid_prerelease);
    expect_error(cc::Version::create(1, 0, 0, "", "build!"), cc::VersionError::invalid_build);
}

TEST(Part8Version, PrecedenceFollowsSemverExample) {
    const std::vector<cc::Version> ordered{
        version("1.0.0-alpha"),  version("1.0.0-alpha.1"), version("1.0.0-alpha.beta"),
        version("1.0.0-beta"),   version("1.0.0-beta.2"),   version("1.0.0-beta.11"),
        version("1.0.0-rc.1"),   version("1.0.0"),
    };
    for (std::size_t i = 1; i < ordered.size(); ++i) {
        EXPECT_LT(ordered[i - 1], ordered[i]) << "at index " << i;
    }
    EXPECT_LT(version("1.0.0-1"), version("1.0.0-alpha"));
}

TEST(Part8Version, CoreComparisonIsNumeric) {
    EXPECT_GT(version("1.10.0"), version("1.9.0"));
    EXPECT_GT(version("2.0.0"), version("1.99.99"));
}

TEST(Part8Version, BuildMetadataIgnoredForEqualityAndHashButKeptInText) {
    const auto a = version("1.0.0+a");
    const auto b = version("1.0.0+b");
    EXPECT_EQ(a, b);
    EXPECT_EQ(std::hash<cc::Version>{}(a), std::hash<cc::Version>{}(b));
    EXPECT_EQ(a.to_string(), "1.0.0+a");

    std::ostringstream out;
    out << version("2.1.0-rc.1+sha.5");
    EXPECT_EQ(out.str(), "2.1.0-rc.1+sha.5");

    std::unordered_set<cc::Version> seen{a, b};
    EXPECT_EQ(seen.size(), 1u);
}

TEST(Part8Version, BumpResetsLowerPartsAndClearsPrerelease) {
    const auto base = version("1.2.3-rc.1+build.9");
    EXPECT_EQ(base.bump(cc::BumpPart::minor).value(), version("1.3.0"));
    EXPECT_EQ(base.bump(cc::BumpPart::major).value(), version("2.0.0"));
    EXPECT_EQ(base.bump(cc::BumpPart::patch).value().to_string(), "1.2.4");
}

TEST(Part8Version, BumpOverflowIsReported) {
    const auto huge = cc::Version::create(INT_MAX, 0, 0).value();
    expect_error(huge.bump(cc::BumpPart::major), cc::VersionError::overflow);
}
