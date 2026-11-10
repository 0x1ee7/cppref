// Dependency-free digests and checksums: streaming SHA-256 (FIPS 180-4) and
// CRC-32 (IEEE 802.3, the zip/gzip/PNG polynomial).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace cppref::files {

using Sha256Digest = std::array<std::uint8_t, 32>;

// Streaming SHA-256. update() may be called any number of times with any split of
// the input; the digest depends only on the concatenated bytes. finish() may be called
// once; the object must not be updated afterwards.
class Sha256 {
public:
    Sha256();
    void update(std::span<const std::byte> data);
    void update(std::string_view text);
    [[nodiscard]] Sha256Digest finish();

private:
    void absorb(const std::uint8_t* data, std::size_t size);
    void compress(const std::uint8_t* block);

    std::array<std::uint32_t, 8> state_{};
    std::array<std::uint8_t, 64> buffer_{};
    std::size_t buffered_ = 0;
    std::uint64_t total_bytes_ = 0;
    bool finished_ = false;
};

// Lowercase hex, two characters per byte.
[[nodiscard]] std::string to_hex(std::span<const std::uint8_t> bytes);

// One-shot helpers returning lowercase hex.
[[nodiscard]] std::string sha256_hex(std::span<const std::byte> data);
[[nodiscard]] std::string sha256_hex(std::string_view text);

// Digests a file read in binary mode in chunks of `chunk_size` bytes; the file is never
// held in memory whole.
//   chunk_size == 0            -> std::invalid_argument
//   file cannot be opened/read -> std::filesystem::filesystem_error
[[nodiscard]] std::string sha256_file(const std::filesystem::path& path, std::size_t chunk_size = 65'536);

// CRC-32, reflected, polynomial 0xEDB88320, initial and final XOR 0xFFFFFFFF.
// `seed` is the value returned for the preceding bytes, so
// crc32(b, crc32(a)) == crc32(a + b). crc32("123456789") == 0xCBF43926.
[[nodiscard]] std::uint32_t crc32(std::span<const std::byte> data, std::uint32_t seed = 0);

}  // namespace cppref::files
