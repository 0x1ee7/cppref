// Binary snapshot container: big-endian header with a CRC-32 over the payload.
#pragma once

#include "timestamp.hpp"

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace cppref::files {

class SnapshotError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class UnsupportedVersionError : public SnapshotError {
public:
    explicit UnsupportedVersionError(std::uint16_t version)
        : SnapshotError("unsupported snapshot version " + std::to_string(version)),
          version_(version) {}
    [[nodiscard]] std::uint16_t version() const noexcept { return version_; }

private:
    std::uint16_t version_;
};

inline constexpr char snapshot_magic[4] = {'C', 'P', 'S', 'N'};
inline constexpr std::uint16_t snapshot_version = 2;
inline constexpr std::size_t snapshot_header_size_v1 = 12;
inline constexpr std::size_t snapshot_header_size_v2 = 24;

// Encodes a value of integral type T as sizeof(T) big-endian bytes.
// Byte order is fixed by the format and independent of the host.
template <std::integral T>
[[nodiscard]] std::array<std::byte, sizeof(T)> to_big_endian(T value) {
    static_assert(!std::same_as<T, bool>, "bool has no portable wire encoding here");
    using U = std::make_unsigned_t<T>;
    U u = static_cast<U>(value);
    if constexpr (std::endian::native == std::endian::little) {
        u = std::byteswap(u);
    }
    std::array<std::byte, sizeof(T)> out{};
    std::memcpy(out.data(), &u, sizeof(T));
    return out;
}

// Decodes sizeof(T) big-endian bytes. Size mismatch -> std::invalid_argument.
template <std::integral T>
[[nodiscard]] T from_big_endian(std::span<const std::byte> bytes) {
    static_assert(!std::same_as<T, bool>, "bool has no portable wire encoding here");
    if (bytes.size() != sizeof(T)) {
        throw std::invalid_argument("from_big_endian: wrong byte count");
    }
    using U = std::make_unsigned_t<T>;
    U u{};
    std::memcpy(&u, bytes.data(), sizeof(T));
    if constexpr (std::endian::native == std::endian::little) {
        u = std::byteswap(u);
    }
    return static_cast<T>(u);
}

// Decoded snapshot. For version 1, created_at is empty and the CRC is absent.
struct Snapshot {
    std::uint16_t version = snapshot_version;
    std::uint16_t flags = 0;
    std::uint32_t entry_count = 0;
    std::optional<Timestamp> created_at;
    std::vector<std::byte> payload;
};

// Layout (all integers big-endian):
//   off  size  field
//    0     4   magic "CPSN"
//    4     2   version (2)
//    6     2   flags
//    8     4   entry_count
//   12     8   created_us  (int64 microseconds since the epoch)
//   20     4   crc32(payload)
//   24     *   payload
[[nodiscard]] std::vector<std::byte> encode_snapshot(std::uint32_t entry_count,
                                                     Timestamp created_at,
                                                     std::span<const std::byte> payload,
                                                     std::uint16_t flags = 0);

// Accepts version 1 (12-byte header, no created_us/CRC; the payload is the rest of the
// input) and version 2 (checked against the CRC).
//   size < 12, or < 24 for version 2     -> SnapshotError("truncated ...")
//   magic mismatch                       -> SnapshotError("bad magic ...")
//   CRC mismatch (v2)                    -> SnapshotError("checksum mismatch ...")
//   any other version                    -> UnsupportedVersionError
[[nodiscard]] Snapshot decode_snapshot(std::span<const std::byte> bytes);

// encode_snapshot followed by write_file_atomic.
void write_snapshot(const std::filesystem::path& path, std::uint32_t entry_count,
                    Timestamp created_at, std::span<const std::byte> payload,
                    std::uint16_t flags = 0);

// Reads the whole file and runs decode_snapshot. Missing file -> filesystem_error.
[[nodiscard]] Snapshot read_snapshot(const std::filesystem::path& path);

}  // namespace cppref::files
