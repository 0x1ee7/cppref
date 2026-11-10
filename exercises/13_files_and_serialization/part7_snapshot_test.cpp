// Part 7: endian-safe binary snapshot header with CRC (snapshot.hpp).
#include "digest.hpp"
#include "snapshot.hpp"
#include "test_support_test.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using cppref::files::SnapshotError;
using cppref::files::Timestamp;
using cppref::files::UnsupportedVersionError;
using cppref::files::crc32;
using cppref::files::decode_snapshot;
using cppref::files::encode_snapshot;
using cppref::files::from_big_endian;
using cppref::files::read_snapshot;
using cppref::files::to_big_endian;
using cppref::files::write_snapshot;
using cppref::files::testing::TempDir;

namespace {

Timestamp when() {
    return Timestamp{std::chrono::microseconds{1'709'210'096'000'123}};
}

std::vector<std::byte> bytes_of(std::initializer_list<unsigned> values) {
    std::vector<std::byte> out;
    for (const unsigned v : values) {
        out.push_back(static_cast<std::byte>(v));
    }
    return out;
}

std::vector<std::byte> payload_abc() {
    return bytes_of({0x61, 0x62, 0x63});
}

}  // namespace

TEST(Part7BigEndian, EncodesMostSignificantByteFirst) {
    const auto bytes = to_big_endian(std::uint32_t{0x01020304});
    EXPECT_EQ(bytes, (std::array<std::byte, 4>{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}}));
}

TEST(Part7BigEndian, SignedValuesUseTwosComplementPattern) {
    const auto bytes = to_big_endian(std::int16_t{-2});
    EXPECT_EQ(bytes, (std::array<std::byte, 2>{std::byte{0xff}, std::byte{0xfe}}));
}

TEST(Part7BigEndian, DecodesIndependentOfHostOrder) {
    const std::array<std::byte, 2> wire{std::byte{0x12}, std::byte{0x34}};
    EXPECT_EQ(from_big_endian<std::uint16_t>(wire), 0x1234u);
    EXPECT_EQ(from_big_endian<std::int16_t>(wire), 0x1234);
}

TEST(Part7BigEndian, RoundTripsExtremeValues) {
    EXPECT_EQ(from_big_endian<std::int64_t>(to_big_endian(std::int64_t{-1})), -1);
    EXPECT_EQ(from_big_endian<std::uint64_t>(to_big_endian(std::uint64_t{0xDEADBEEFCAFEF00DULL})),
              0xDEADBEEFCAFEF00DULL);
}

TEST(Part7BigEndian, WrongByteCountIsInvalidArgument) {
    const std::array<std::byte, 3> three{};
    EXPECT_THROW(static_cast<void>(from_big_endian<std::uint32_t>(three)), std::invalid_argument);
}

TEST(Part7Format, EncodedHeaderMatchesDocumentedLayout) {
    const auto bytes = encode_snapshot(3, when(), payload_abc(), 0);
    ASSERT_EQ(bytes.size(), 24u + 3u);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(bytes.data()), 4), "CPSN");
    EXPECT_EQ(bytes[4], std::byte{0x00});
    EXPECT_EQ(bytes[5], std::byte{0x02});
    EXPECT_EQ(bytes[8], std::byte{0x00});
    EXPECT_EQ(bytes[11], std::byte{0x03});
    const auto crc = crc32(payload_abc());
    const auto crc_bytes = to_big_endian(crc);
    for (std::size_t i = 0; i < 4; ++i) {
        EXPECT_EQ(bytes[20 + i], crc_bytes[i]);
    }
}

TEST(Part7Format, EncodeThenDecodeRoundTrips) {
    const auto bytes = encode_snapshot(3, when(), payload_abc(), 0x0005);
    const auto snap = decode_snapshot(bytes);
    EXPECT_EQ(snap.version, 2u);
    EXPECT_EQ(snap.flags, 0x0005u);
    EXPECT_EQ(snap.entry_count, 3u);
    ASSERT_TRUE(snap.created_at.has_value());
    EXPECT_EQ(*snap.created_at, when());
    EXPECT_EQ(snap.payload, payload_abc());
}

TEST(Part7Format, DecodesVersionOneWithoutCreatedAtOrCrc) {
    // v1: "CPSN", version 1, flags 0, entry_count 2, then payload directly.
    const auto bytes = bytes_of({'C', 'P', 'S', 'N', 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 9, 8});
    const auto snap = decode_snapshot(bytes);
    EXPECT_EQ(snap.version, 1u);
    EXPECT_EQ(snap.entry_count, 2u);
    EXPECT_FALSE(snap.created_at.has_value());
    EXPECT_EQ(snap.payload, bytes_of({9, 8}));
}

TEST(Part7Errors, BadMagicIsSnapshotError) {
    auto bytes = encode_snapshot(0, when(), {}, 0);
    bytes[0] = std::byte{'X'};
    try {
        static_cast<void>(decode_snapshot(bytes));
        FAIL() << "expected SnapshotError";
    } catch (const SnapshotError& e) {
        EXPECT_NE(std::string(e.what()).find("magic"), std::string::npos) << e.what();
    }
}

TEST(Part7Errors, ShortInputIsTruncated) {
    const auto bytes = bytes_of({'C', 'P', 'S'});
    try {
        static_cast<void>(decode_snapshot(bytes));
        FAIL() << "expected SnapshotError";
    } catch (const SnapshotError& e) {
        EXPECT_NE(std::string(e.what()).find("truncated"), std::string::npos) << e.what();
    }
}

TEST(Part7Errors, VersionTwoHeaderCutShortIsTruncated) {
    auto bytes = encode_snapshot(0, when(), {}, 0);
    bytes.resize(20);
    try {
        static_cast<void>(decode_snapshot(bytes));
        FAIL() << "expected SnapshotError";
    } catch (const SnapshotError& e) {
        EXPECT_NE(std::string(e.what()).find("truncated"), std::string::npos) << e.what();
    }
}

TEST(Part7Errors, UnknownVersionIsUnsupportedWithVersionNumber) {
    auto bytes = encode_snapshot(0, when(), {}, 0);
    bytes[5] = std::byte{0x07};
    try {
        static_cast<void>(decode_snapshot(bytes));
        FAIL() << "expected UnsupportedVersionError";
    } catch (const UnsupportedVersionError& e) {
        EXPECT_EQ(e.version(), 7u);
    }
}

TEST(Part7Errors, PayloadCorruptionIsDetectedByChecksum) {
    auto bytes = encode_snapshot(3, when(), payload_abc(), 0);
    bytes.back() = std::byte{0x7f};
    try {
        static_cast<void>(decode_snapshot(bytes));
        FAIL() << "expected SnapshotError";
    } catch (const SnapshotError& e) {
        EXPECT_NE(std::string(e.what()).find("checksum"), std::string::npos) << e.what();
    }
}

TEST(Part7File, WriteThenReadRoundTripsThroughDisk) {
    TempDir dir;
    write_snapshot(dir.path() / "state.cpsn", 3, when(), payload_abc(), 0);
    const auto snap = read_snapshot(dir.path() / "state.cpsn");
    EXPECT_EQ(snap.entry_count, 3u);
    ASSERT_TRUE(snap.created_at.has_value());
    EXPECT_EQ(*snap.created_at, when());
    EXPECT_EQ(snap.payload, payload_abc());
}
