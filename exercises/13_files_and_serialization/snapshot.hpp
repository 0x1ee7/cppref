// Versioned binary snapshot with an explicit big-endian header and a payload CRC.
#pragma once

#include "timestamp.hpp"

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace cppref::files {

// Bad magic, truncated input, checksum mismatch.
class SnapshotError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class UnsupportedVersionError : public SnapshotError {
public:
    explicit UnsupportedVersionError(std::uint16_t version)
        : SnapshotError("unsupported snapshot version " + std::to_string(version)), version_(version) {}

    [[nodiscard]] std::uint16_t version() const noexcept { return version_; }

private:
    std::uint16_t version_;
};

inline constexpr std::string_view snapshot_magic = "CPSN";
inline constexpr std::uint16_t snapshot_version = 2;
inline constexpr std::size_t snapshot_header_size_v1 = 12;
inline constexpr std::size_t snapshot_header_size_v2 = 24;

// Integers are stored big-endian (network order) whatever the host byte order is.
// Signed values are stored as their two's-complement bit pattern.
template <std::integral T>
[[nodiscard]] std::array<std::byte, sizeof(T)> to_big_endian(T) {
    throw std::logic_error("not implemented: to_big_endian");
}

// bytes.size() must equal sizeof(T); otherwise std::invalid_argument.
template <std::integral T>
[[nodiscard]] T from_big_endian(std::span<const std::byte>) {
    throw std::logic_error("not implemented: from_big_endian");
}

struct Snapshot {
    std::uint16_t version = 0;
    std::uint16_t flags = 0;
    std::uint32_t entry_count = 0;
    std::optional<Timestamp> created_at;  // empty for version 1 (not recorded)
    std::vector<std::byte> payload;
};

// Encodes a version-2 snapshot (all integers big-endian):
//   offset  0  magic "CPSN"                                  4 bytes
//   offset  4  version (u16)                                 2
//   offset  6  flags (u16)                                   2
//   offset  8  entry_count (u32)                             4
//   offset 12  created_at, microseconds since epoch (i64)    8
//   offset 20  crc32(payload) (u32)                          4
//   offset 24  payload
[[nodiscard]] std::vector<std::byte> encode_snapshot(std::uint32_t entry_count,
                                                     Timestamp created_at,
                                                     std::span<const std::byte> payload,
                                                     std::uint16_t flags = 0);

// Decodes version 1 (12-byte header, no created_at, no CRC) or version 2.
// Checks, in order:
//   fewer than 12 bytes, or fewer than the version's header size -> SnapshotError ("truncated ...");
//   magic is not "CPSN"                                          -> SnapshotError ("bad magic ...");
//   version is neither 1 nor 2                                   -> UnsupportedVersionError;
//   v2 payload CRC mismatch                                      -> SnapshotError ("checksum mismatch ...").
[[nodiscard]] Snapshot decode_snapshot(std::span<const std::byte> bytes);

// File wrappers: write via write_file_atomic; read via read_file_binary + decode_snapshot.
void write_snapshot(const std::filesystem::path& path,
                    std::uint32_t entry_count,
                    Timestamp created_at,
                    std::span<const std::byte> payload,
                    std::uint16_t flags = 0);

[[nodiscard]] Snapshot read_snapshot(const std::filesystem::path& path);

}  // namespace cppref::files
