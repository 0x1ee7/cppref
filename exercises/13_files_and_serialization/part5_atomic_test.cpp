// Part 5: atomic whole-file writes and binary-safe reads (file_io.hpp, manifest persistence).
#include "file_io.hpp"
#include "manifest_store.hpp"
#include "test_support_test.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

using cppref::files::FileEntry;
using cppref::files::Manifest;
using cppref::files::load_manifest;
using cppref::files::read_file_binary;
using cppref::files::save_manifest_atomic;
using cppref::files::write_file_atomic;
using cppref::files::testing::TempDir;
using cppref::files::testing::child_names;
using cppref::files::testing::read_bytes;
using cppref::files::testing::write_bytes;

TEST(Part5Atomic, WriteThenReadRoundTripsBinaryBytes) {
    TempDir dir;
    const std::string data("\x00\x01\xff\r\nend", 8);
    write_file_atomic(dir.path() / "out.bin", data);
    EXPECT_EQ(read_file_binary(dir.path() / "out.bin"), data);
}

TEST(Part5Atomic, EmptyDataCreatesEmptyFile) {
    TempDir dir;
    write_file_atomic(dir.path() / "empty.txt", "");
    EXPECT_TRUE(std::filesystem::exists(dir.path() / "empty.txt"));
    EXPECT_EQ(std::filesystem::file_size(dir.path() / "empty.txt"), 0u);
}

TEST(Part5Atomic, OverwriteReplacesPreviousContents) {
    TempDir dir;
    write_bytes(dir.path() / "f.txt", "old contents that are longer");
    write_file_atomic(dir.path() / "f.txt", "new");
    EXPECT_EQ(read_bytes(dir.path() / "f.txt"), "new");
}

TEST(Part5Atomic, LeavesNoTemporaryFilesBehind) {
    TempDir dir;
    write_file_atomic(dir.path() / "out.bin", "data");
    write_file_atomic(dir.path() / "out.bin", "more data");
    EXPECT_EQ(child_names(dir.path()), (std::vector<std::string>{"out.bin"}));
}

TEST(Part5Atomic, MissingParentDirectoryThrowsAndCreatesNothing) {
    TempDir dir;
    EXPECT_THROW(write_file_atomic(dir.path() / "no-such-dir" / "f.txt", "x"),
                 std::filesystem::filesystem_error);
    EXPECT_FALSE(std::filesystem::exists(dir.path() / "no-such-dir"));
}

TEST(Part5Atomic, TargetThatIsADirectoryThrowsAndLeavesNoTemporary) {
    TempDir dir;
    std::filesystem::create_directory(dir.path() / "target");
    EXPECT_THROW(write_file_atomic(dir.path() / "target", "x"), std::filesystem::filesystem_error);
    EXPECT_EQ(child_names(dir.path()), (std::vector<std::string>{"target"}));
}

TEST(Part5Atomic, ReadMissingFileThrowsFilesystemError) {
    TempDir dir;
    EXPECT_THROW(static_cast<void>(read_file_binary(dir.path() / "missing.bin")), std::filesystem::filesystem_error);
}

TEST(Part5Manifest, SaveAndLoadRoundTripsThroughDisk) {
    TempDir dir;
    Manifest m{{"a.txt", FileEntry{2, cppref::files::Timestamp{std::chrono::microseconds{7}}, std::string(64, 'e')}}};
    save_manifest_atomic(m, dir.path() / "manifest.json");
    EXPECT_EQ(load_manifest(dir.path() / "manifest.json"), m);
    EXPECT_EQ(child_names(dir.path()), (std::vector<std::string>{"manifest.json"}));
}

TEST(Part5Manifest, LoadMissingManifestThrowsFilesystemError) {
    TempDir dir;
    EXPECT_THROW(static_cast<void>(load_manifest(dir.path() / "manifest.json")), std::filesystem::filesystem_error);
}
