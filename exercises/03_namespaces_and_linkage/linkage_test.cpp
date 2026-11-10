#include "linkage.hpp"

#include <gtest/gtest.h>

#include <string>

using namespace cppref::linkage;

// --- Part2: inline functions/variables, audit lines and construct-on-first-use -

TEST(Part2Linkage, InlineHitCounterIncrementsEachCall) {
    const int before = hit_counter();
    const int after = hit_counter();
    EXPECT_EQ(after, before + 1);
}

TEST(Part2Linkage, AuditLineFormatsComponentAndMessage) {
    EXPECT_EQ(audit_line("db", "connected"), "[db] connected");
}

TEST(Part2Linkage, AuditLineTruncatesToInlineLimit) {
    const std::string line = audit_line("c", std::string(500, 'x'));
    EXPECT_EQ(line.size(), kMaxAuditLine);
    EXPECT_EQ(line.substr(line.size() - 3), "...");
}

TEST(Part2Linkage, AuditLineExactlyAtLimitIsNotTruncated) {
    // "[c] " is 4 characters, so a 116-character message gives exactly kMaxAuditLine.
    const std::string message(kMaxAuditLine - 4, 'm');
    EXPECT_EQ(audit_line("c", message), "[c] " + message);
}

TEST(Part2Linkage, ErrorCatalogIsSingleton) {
    EXPECT_EQ(&error_catalog(), &error_catalog());
}

TEST(Part2Linkage, ErrorCatalogFindsKnownCodes) {
    const auto& catalog = error_catalog();
    ASSERT_TRUE(catalog.find(404).has_value());
    EXPECT_EQ(*catalog.find(404), "Not Found");
    EXPECT_EQ(*catalog.find(500), "Internal Server Error");
}

TEST(Part2Linkage, ErrorCatalogUnknownCodeIsNullopt) {
    EXPECT_FALSE(error_catalog().find(999).has_value());
    EXPECT_EQ(error_catalog().size(), 3u);
}
