// Part 1: SHA-256 and CRC-32 (digest.hpp).
#include "digest.hpp"
#include "test_support_test.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

using cppref::files::Sha256;
using cppref::files::Sha256Digest;
using cppref::files::crc32;
using cppref::files::sha256_file;
using cppref::files::sha256_hex;
using cppref::files::to_hex;
using cppref::files::testing::TempDir;
using cppref::files::testing::write_bytes;

namespace {

std::span<const std::byte> as_bytes_of(std::string_view text) {
    return std::as_bytes(std::span(text));
}

}  // namespace

TEST(Part1Digest, Sha256OfEmptyMessage) {
    EXPECT_EQ(sha256_hex(std::string_view{""}),
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(Part1Digest, Sha256OfAbc) {
    EXPECT_EQ(sha256_hex(std::string_view{"abc"}),
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(Part1Digest, Sha256OfTwoBlockMessage) {
    EXPECT_EQ(sha256_hex(std::string_view{"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"}),
              "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST(Part1Digest, Sha256OfOneMillionAs) {
    const std::string million(1'000'000, 'a');
    EXPECT_EQ(sha256_hex(std::string_view{million}),
              "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST(Part1Digest, SpanOverloadMatchesStringViewOverload) {
    EXPECT_EQ(sha256_hex(as_bytes_of("abc")), sha256_hex(std::string_view{"abc"}));
}

TEST(Part1Digest, StreamingSplitsDoNotChangeTheDigest) {
    Sha256 hasher;
    hasher.update(std::string_view{"ab"});
    hasher.update(std::string_view{""});
    hasher.update(std::string_view{"c"});
    EXPECT_EQ(to_hex(hasher.finish()), sha256_hex(std::string_view{"abc"}));
}

TEST(Part1Digest, StreamingOneByteAtATimeMatchesOneShot) {
    const std::string text = "The quick brown fox jumps over the lazy dog";
    Sha256 hasher;
    for (const char c : text) {
        hasher.update(std::string_view{&c, 1});
    }
    const Sha256Digest digest = hasher.finish();
    EXPECT_EQ(to_hex(digest), sha256_hex(std::string_view{text}));
}

TEST(Part1Digest, ToHexIsLowercaseTwoDigitsPerByte) {
    const std::uint8_t bytes[] = {0x00, 0xff, 0x10, 0xab};
    EXPECT_EQ(to_hex(bytes), "00ff10ab");
}

TEST(Part1Digest, Crc32OfCheckString) {
    EXPECT_EQ(crc32(as_bytes_of("123456789")), 0xCBF43926u);
}

TEST(Part1Digest, Crc32OfEmptyInputIsZero) {
    EXPECT_EQ(crc32(std::span<const std::byte>{}), 0u);
}

TEST(Part1Digest, Crc32SeedChainsAcrossSplits) {
    const std::string whole = "hello, checksum world";
    const std::uint32_t first = crc32(as_bytes_of(std::string_view{whole}.substr(0, 7)));
    const std::uint32_t chained = crc32(as_bytes_of(std::string_view{whole}.substr(7)), first);
    EXPECT_EQ(chained, crc32(as_bytes_of(whole)));
}

TEST(Part1Digest, FileDigestMatchesInMemoryDigestForAnyChunkSize) {
    TempDir dir;
    std::string content;
    for (int i = 0; i < 300'000; ++i) {
        content.push_back(static_cast<char>(i % 251));  // binary, includes NUL bytes
    }
    const auto file = dir.path() / "blob.bin";
    write_bytes(file, content);

    const std::string expected = sha256_hex(as_bytes_of(content));
    EXPECT_EQ(sha256_file(file, 1), expected);
    EXPECT_EQ(sha256_file(file, 4096), expected);
    EXPECT_EQ(sha256_file(file), expected);  // default chunk size
}

TEST(Part1Digest, FileDigestRejectsZeroChunkSize) {
    TempDir dir;
    const auto file = dir.path() / "a.txt";
    write_bytes(file, "x");
    EXPECT_THROW(static_cast<void>(sha256_file(file, 0)), std::invalid_argument);
}

TEST(Part1Digest, FileDigestOfMissingFileThrowsFilesystemError) {
    TempDir dir;
    EXPECT_THROW(static_cast<void>(sha256_file(dir.path() / "missing.bin")), std::filesystem::filesystem_error);
}
