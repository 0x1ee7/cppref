// Part 2: glob matching, ignore rules and tree scanning (scan.hpp).
#include "scan.hpp"
#include "digest.hpp"
#include "test_support_test.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

using cppref::files::FileEntry;
using cppref::files::glob_match;
using cppref::files::is_ignored;
using cppref::files::scan_tree;
using cppref::files::sha256_hex;
using cppref::files::testing::TempDir;
using cppref::files::testing::write_bytes;

TEST(Part2Glob, StarStaysWithinOneSegment) {
    EXPECT_TRUE(glob_match("*.cpp", "main.cpp"));
    EXPECT_FALSE(glob_match("*.cpp", "src/main.cpp"));
}

TEST(Part2Glob, QuestionMarkMatchesExactlyOneCharacter) {
    EXPECT_TRUE(glob_match("a?c", "abc"));
    EXPECT_FALSE(glob_match("a?c", "ac"));
    EXPECT_FALSE(glob_match("a?c", "abbc"));
}

TEST(Part2Glob, DoubleStarMatchesZeroSegments) {
    EXPECT_TRUE(glob_match("src/**/x.hpp", "src/x.hpp"));
}

TEST(Part2Glob, DoubleStarMatchesManySegments) {
    EXPECT_TRUE(glob_match("src/**/x.hpp", "src/a/b/x.hpp"));
    EXPECT_FALSE(glob_match("src/**/x.hpp", "lib/a/x.hpp"));
}

TEST(Part2Glob, BracketsAreLiteral) {
    EXPECT_TRUE(glob_match("a[bc].txt", "a[bc].txt"));
    EXPECT_FALSE(glob_match("a[bc].txt", "ab.txt"));
}

TEST(Part2Ignore, NoSlashPatternMatchesAnyComponent) {
    const std::vector<std::string> ignore{"*.tmp"};
    EXPECT_TRUE(is_ignored("a.tmp", ignore));
    EXPECT_TRUE(is_ignored("x/y/b.tmp", ignore));
    EXPECT_FALSE(is_ignored("a.txt", ignore));
}

TEST(Part2Ignore, NoSlashPatternMatchesDirectoryNames) {
    const std::vector<std::string> ignore{".git"};
    EXPECT_TRUE(is_ignored(".git/config", ignore));
    EXPECT_TRUE(is_ignored("pkg/.git/HEAD", ignore));
    EXPECT_FALSE(is_ignored("gitx/a", ignore));
}

TEST(Part2Ignore, SlashPatternIsAnchoredAtRoot) {
    const std::vector<std::string> ignore{"logs/**"};
    EXPECT_TRUE(is_ignored("logs/a.log", ignore));
    EXPECT_FALSE(is_ignored("other/logs/c.log", ignore));
}

TEST(Part2Ignore, EmptyRuleListIgnoresNothing) {
    EXPECT_FALSE(is_ignored("anything/at/all.txt", {}));
}

TEST(Part2Scan, RecordsSizeDigestAndPosixRelativeKeys) {
    TempDir dir;
    write_bytes(dir.path() / "a" / "b.txt", "hello");
    write_bytes(dir.path() / "root.txt", "xyz");

    const auto manifest = scan_tree(dir.path());
    ASSERT_EQ(manifest.size(), 2u);
    ASSERT_TRUE(manifest.contains("a/b.txt"));
    ASSERT_TRUE(manifest.contains("root.txt"));
    EXPECT_EQ(manifest.at("a/b.txt").size, 5u);
    EXPECT_EQ(manifest.at("a/b.txt").sha256, sha256_hex(std::string_view{"hello"}));
    EXPECT_EQ(manifest.at("root.txt").size, 3u);
}

TEST(Part2Scan, MtimeIsCurrentWallClockTime) {
    TempDir dir;
    write_bytes(dir.path() / "fresh.txt", "now");

    const auto manifest = scan_tree(dir.path());
    const auto now = std::chrono::system_clock::now();
    const auto mtime = manifest.at("fresh.txt").mtime;
    const auto skew = now - mtime;
    EXPECT_LT(skew, std::chrono::minutes(10));
    EXPECT_GT(skew, -std::chrono::minutes(10));
}

TEST(Part2Scan, AppliesIgnoreRules) {
    TempDir dir;
    write_bytes(dir.path() / "keep.txt", "k");
    write_bytes(dir.path() / "junk.tmp", "t");
    write_bytes(dir.path() / "sub" / "deep.tmp", "t");
    write_bytes(dir.path() / "logs" / "a.log", "l");
    write_bytes(dir.path() / "other" / "logs" / "c.log", "c");

    const auto manifest = scan_tree(dir.path(), {"*.tmp", "logs/**"});
    std::vector<std::string> keys;
    for (const auto& [key, entry] : manifest) {
        keys.push_back(key);
    }
    EXPECT_EQ(keys, (std::vector<std::string>{"keep.txt", "other/logs/c.log"}));
}

TEST(Part2Scan, SkipsSymlinksToFilesAndDirectories) {
    TempDir dir;
    write_bytes(dir.path() / "real.txt", "data");
    EXPECT_TRUE(scan_tree(dir.path()).contains("real.txt"));
    std::error_code ec;
    std::filesystem::create_directory_symlink(dir.path() / "real.txt", dir.path() / "file_link", ec);
    if (ec) {
        GTEST_SKIP() << "cannot create symlinks on this host: " << ec.message();
    }
    std::filesystem::create_directory(dir.path() / "realdir");
    write_bytes(dir.path() / "realdir" / "inside.txt", "in");
    std::filesystem::create_directory_symlink(dir.path() / "realdir", dir.path() / "dir_link", ec);

    const auto manifest = scan_tree(dir.path());
    EXPECT_TRUE(manifest.contains("real.txt"));
    EXPECT_FALSE(manifest.contains("file_link"));
    EXPECT_FALSE(manifest.contains("dir_link/inside.txt"));
    EXPECT_TRUE(manifest.contains("realdir/inside.txt"));
}

TEST(Part2Scan, EmptyDirectoryYieldsEmptyManifest) {
    TempDir dir;
    EXPECT_TRUE(scan_tree(dir.path()).empty());
}

TEST(Part2Scan, MissingRootThrowsFilesystemError) {
    TempDir dir;
    EXPECT_THROW(static_cast<void>(scan_tree(dir.path() / "does-not-exist")), std::filesystem::filesystem_error);
}

TEST(Part2Scan, FileAsRootThrowsFilesystemError) {
    TempDir dir;
    write_bytes(dir.path() / "plain.txt", "p");
    EXPECT_THROW(static_cast<void>(scan_tree(dir.path() / "plain.txt")), std::filesystem::filesystem_error);
}
