// Part 3: manifest diff and traversal-safe apply (sync.hpp).
#include "scan.hpp"
#include "sync.hpp"
#include "test_support_test.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

using cppref::files::ApplyResult;
using cppref::files::FileEntry;
using cppref::files::Manifest;
using cppref::files::ManifestDiff;
using cppref::files::apply_diff;
using cppref::files::diff_manifests;
using cppref::files::validate_relative_key;
using cppref::files::testing::TempDir;
using cppref::files::testing::child_names;
using cppref::files::testing::read_bytes;
using cppref::files::testing::write_bytes;

namespace {

FileEntry entry(std::uint64_t size, std::string sha, std::int64_t mtime_us = 1'000'000) {
    return FileEntry{size, cppref::files::Timestamp{std::chrono::microseconds{mtime_us}}, std::move(sha)};
}

}  // namespace

TEST(Part3Diff, ClassifiesAddedRemovedAndModified) {
    Manifest old_m{{"keep.txt", entry(1, "aa")}, {"gone.txt", entry(1, "bb")}, {"edit.txt", entry(1, "cc")}};
    Manifest new_m{{"keep.txt", entry(1, "aa")}, {"edit.txt", entry(2, "dd")}, {"new.txt", entry(1, "ee")}};

    const ManifestDiff diff = diff_manifests(old_m, new_m);
    EXPECT_EQ(diff.added, (std::vector<std::string>{"new.txt"}));
    EXPECT_EQ(diff.removed, (std::vector<std::string>{"gone.txt"}));
    EXPECT_EQ(diff.modified, (std::vector<std::string>{"edit.txt"}));
    EXPECT_FALSE(diff.empty());
}

TEST(Part3Diff, MtimeOnlyChangeIsNotAModification) {
    Manifest old_m{{"a.txt", entry(4, "aa", 1'000'000)}};
    Manifest new_m{{"a.txt", entry(4, "aa", 9'000'000)}};

    const ManifestDiff diff = diff_manifests(old_m, new_m);
    EXPECT_TRUE(diff.modified.empty());
    EXPECT_TRUE(diff.empty());
}

TEST(Part3Diff, IdenticalManifestsProduceEmptyDiff) {
    Manifest m{{"a.txt", entry(1, "aa")}};
    EXPECT_EQ(diff_manifests(m, m), ManifestDiff{});
    EXPECT_TRUE(diff_manifests(m, m).empty());
}

TEST(Part3Keys, AcceptsPlainRelativeKeys) {
    EXPECT_NO_THROW(validate_relative_key("a.txt"));
    EXPECT_NO_THROW(validate_relative_key("dir/sub/file.bin"));
    EXPECT_NO_THROW(validate_relative_key("dir.v2/x..y"));
}

TEST(Part3Keys, RejectsAbsoluteEmptyAndTraversalKeys) {
    for (const std::string bad : {std::string{""}, std::string{"/etc/passwd"}, std::string{"\\server\\share"},
                                  std::string{"C:/x"}, std::string{"C:x"}, std::string{"a/../b"},
                                  std::string{"a/./b"}, std::string{"a//b"}, std::string{"./a"},
                                  std::string{"a/"}, std::string{".."}}) {
        SCOPED_TRACE("key: " + bad);
        EXPECT_THROW(validate_relative_key(bad), std::invalid_argument);
    }
}

TEST(Part3Keys, BackslashTraversalIsRejectedOnEveryPlatform) {
    EXPECT_THROW(validate_relative_key("a\\..\\b"), std::invalid_argument);
}

TEST(Part3Apply, CopiesAddedAndModifiedDeletesRemoved) {
    TempDir src_dir;
    TempDir dst_dir;
    write_bytes(src_dir.path() / "a" / "new.txt", "NEW");
    write_bytes(src_dir.path() / "a" / "changed.txt", "v2");
    write_bytes(dst_dir.path() / "a" / "changed.txt", "v1");
    write_bytes(dst_dir.path() / "gone" / "old.txt", "old");
    write_bytes(dst_dir.path() / "keep.txt", "untouched");

    ManifestDiff diff;
    diff.added = {"a/new.txt"};
    diff.modified = {"a/changed.txt"};
    diff.removed = {"gone/old.txt"};

    const ApplyResult result = apply_diff(diff, src_dir.path(), dst_dir.path());
    EXPECT_EQ(result.copied, (std::vector<std::string>{"a/new.txt", "a/changed.txt"}));
    EXPECT_EQ(result.deleted, (std::vector<std::string>{"gone/old.txt"}));
    EXPECT_EQ(read_bytes(dst_dir.path() / "a" / "new.txt"), "NEW");
    EXPECT_EQ(read_bytes(dst_dir.path() / "a" / "changed.txt"), "v2");
    EXPECT_EQ(read_bytes(dst_dir.path() / "keep.txt"), "untouched");
}

TEST(Part3Apply, PreservesLastWriteTimeOfCopiedFiles) {
    TempDir src_dir;
    TempDir dst_dir;
    write_bytes(src_dir.path() / "f.txt", "payload");
    const auto stamp = std::filesystem::last_write_time(src_dir.path() / "f.txt") - std::chrono::hours(48);
    std::filesystem::last_write_time(src_dir.path() / "f.txt", stamp);

    ManifestDiff diff;
    diff.added = {"f.txt"};
    static_cast<void>(apply_diff(diff, src_dir.path(), dst_dir.path()));

    EXPECT_EQ(std::filesystem::last_write_time(dst_dir.path() / "f.txt"), stamp);
}

TEST(Part3Apply, PrunesEmptyParentsUpToButNotIncludingDest) {
    TempDir dst_dir;
    write_bytes(dst_dir.path() / "x" / "y" / "z.txt", "bye");
    write_bytes(dst_dir.path() / "x" / "kept.txt", "stay");

    ManifestDiff diff;
    diff.removed = {"x/y/z.txt"};
    static_cast<void>(apply_diff(diff, dst_dir.path() / "unused-source", dst_dir.path()));

    EXPECT_FALSE(std::filesystem::exists(dst_dir.path() / "x" / "y"));
    EXPECT_TRUE(std::filesystem::exists(dst_dir.path() / "x" / "kept.txt"));
    EXPECT_TRUE(std::filesystem::is_directory(dst_dir.path()));
}

TEST(Part3Apply, ValidatesEveryKeyBeforeTouchingDisk) {
    TempDir src_dir;
    TempDir dst_dir;
    write_bytes(src_dir.path() / "ok.txt", "fine");
    write_bytes(src_dir.path() / "escape.txt", "nope");

    ManifestDiff diff;
    diff.added = {"ok.txt", "../escape.txt"};

    EXPECT_THROW(static_cast<void>(apply_diff(diff, src_dir.path(), dst_dir.path())), std::invalid_argument);
    EXPECT_TRUE(child_names(dst_dir.path()).empty());
}

TEST(Part3Apply, AbsentRemovedFileIsNotAnError) {
    TempDir src_dir;
    TempDir dst_dir;
    write_bytes(dst_dir.path() / "real.txt", "here");

    ManifestDiff diff;
    diff.removed = {"ghost.txt", "real.txt"};
    const ApplyResult result = apply_diff(diff, src_dir.path(), dst_dir.path());

    EXPECT_EQ(result.deleted, (std::vector<std::string>{"real.txt"}));
    EXPECT_FALSE(std::filesystem::exists(dst_dir.path() / "real.txt"));
}
