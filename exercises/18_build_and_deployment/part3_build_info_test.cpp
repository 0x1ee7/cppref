#include "build_info.hpp"

#include <gtest/gtest.h>

#include <string>

namespace dep = cppref::deployment;

TEST(Part3BuildInfo, ParseVersionAcceptsStrictSemver) {
    auto v = dep::parse_version("10.20.30");
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(v->major, 10u);
    EXPECT_EQ(v->minor, 20u);
    EXPECT_EQ(v->patch, 30u);
    EXPECT_EQ(dep::parse_version("0.1.0")->to_string(), "0.1.0");
}

TEST(Part3BuildInfo, ParseVersionRejectsEverythingElse) {
    for (std::string_view text : {"", "1.2", "1.2.3.4", "v1.2.3", "1.2.3-rc1", "01.2.3", "1..3",
                                  "-1.0.0", "1.2.x", "99999999999.0.0"}) {
        EXPECT_FALSE(dep::parse_version(text).has_value()) << text;
    }
}

TEST(Part3BuildInfo, VersionsOrderNumerically) {
    EXPECT_LT(*dep::parse_version("1.9.9"), *dep::parse_version("1.10.0"));
    EXPECT_LT(*dep::parse_version("1.99.99"), *dep::parse_version("2.0.0"));
    EXPECT_EQ(*dep::parse_version("1.2.3"), *dep::parse_version("1.2.3"));
}

TEST(Part3BuildInfo, GitShaAcceptsSevenToFortyLowercaseHex) {
    EXPECT_TRUE(dep::is_git_sha("abc1234"));
    EXPECT_TRUE(dep::is_git_sha(std::string(40, 'f')));
    EXPECT_TRUE(dep::is_git_sha("0123456789abcdef"));
}

TEST(Part3BuildInfo, GitShaRejectsOtherShapes) {
    EXPECT_FALSE(dep::is_git_sha(""));
    EXPECT_FALSE(dep::is_git_sha("abc123"));
    EXPECT_FALSE(dep::is_git_sha(std::string(41, 'a')));
    EXPECT_FALSE(dep::is_git_sha("ABC1234"));
    EXPECT_FALSE(dep::is_git_sha("abc123g"));
}

TEST(Part3BuildInfo, BuildInfoCarriesStampedValues) {
    const auto info = dep::build_info();
    EXPECT_TRUE(dep::parse_version(info.version).has_value()) << info.version;
    EXPECT_TRUE(info.git_sha == "unknown" || dep::is_git_sha(info.git_sha)) << info.git_sha;
}

TEST(Part3BuildInfo, VersionLineFormat) {
    EXPECT_EQ((dep::BuildInfo{"1.2.3", "abc1234"}).version_line(), "cppref-service 1.2.3 (abc1234)");
    EXPECT_EQ((dep::BuildInfo{"1.2.3", "unknown"}).version_line(), "cppref-service 1.2.3 (unknown)");
}
