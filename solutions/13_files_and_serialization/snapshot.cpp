#include "snapshot.hpp"

#include "digest.hpp"
#include "file_io.hpp"

#include <string>

namespace cppref::files {
namespace fs = std::filesystem;

namespace {

template <std::integral T>
void append_be(std::vector<std::byte>& out, T value) {
    const auto bytes = to_big_endian(value);
    out.insert(out.end(), bytes.begin(), bytes.end());
}

bool magic_matches(std::span<const std::byte> bytes) {
    for (std::size_t i = 0; i < sizeof snapshot_magic; ++i) {
        if (std::to_integer<char>(bytes[i]) != snapshot_magic[i]) {
            return false;
        }
    }
    return true;
}

}  // namespace

std::vector<std::byte> encode_snapshot(std::uint32_t entry_count, Timestamp created_at,
                                       std::span<const std::byte> payload, std::uint16_t flags) {
    std::vector<std::byte> out;
    out.reserve(snapshot_header_size_v2 + payload.size());
    for (const char c : snapshot_magic) {
        out.push_back(static_cast<std::byte>(c));
    }
    append_be<std::uint16_t>(out, snapshot_version);
    append_be<std::uint16_t>(out, flags);
    append_be<std::uint32_t>(out, entry_count);
    append_be<std::int64_t>(out, created_at.time_since_epoch().count());
    append_be<std::uint32_t>(out, crc32(payload));
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

Snapshot decode_snapshot(std::span<const std::byte> bytes) {
    if (bytes.size() < snapshot_header_size_v1) {
        throw SnapshotError("truncated snapshot: " + std::to_string(bytes.size()) + " bytes");
    }
    if (!magic_matches(bytes)) {
        throw SnapshotError("bad magic: not a CPSN snapshot");
    }
    const auto version = from_big_endian<std::uint16_t>(bytes.subspan(4, 2));
    const auto flags = from_big_endian<std::uint16_t>(bytes.subspan(6, 2));
    const auto entry_count = from_big_endian<std::uint32_t>(bytes.subspan(8, 4));

    if (version == 1) {
        Snapshot snap;
        snap.version = version;
        snap.flags = flags;
        snap.entry_count = entry_count;
        const auto payload = bytes.subspan(snapshot_header_size_v1);
        snap.payload.assign(payload.begin(), payload.end());
        return snap;
    }
    if (version != snapshot_version) {
        throw UnsupportedVersionError(version);
    }
    if (bytes.size() < snapshot_header_size_v2) {
        throw SnapshotError("truncated snapshot: " + std::to_string(bytes.size()) + " bytes");
    }

    const auto created_us = from_big_endian<std::int64_t>(bytes.subspan(12, 8));
    const auto expected_crc = from_big_endian<std::uint32_t>(bytes.subspan(20, 4));
    const auto payload = bytes.subspan(snapshot_header_size_v2);
    if (crc32(payload) != expected_crc) {
        throw SnapshotError("checksum mismatch: payload is corrupt");
    }

    Snapshot snap;
    snap.version = version;
    snap.flags = flags;
    snap.entry_count = entry_count;
    snap.created_at = Timestamp{std::chrono::microseconds{created_us}};
    snap.payload.assign(payload.begin(), payload.end());
    return snap;
}

void write_snapshot(const fs::path& path, std::uint32_t entry_count, Timestamp created_at,
                    std::span<const std::byte> payload, std::uint16_t flags) {
    const std::vector<std::byte> bytes = encode_snapshot(entry_count, created_at, payload, flags);
    write_file_atomic(path, std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
}

Snapshot read_snapshot(const fs::path& path) {
    const std::string data = read_file_binary(path);
    return decode_snapshot(std::as_bytes(std::span(data)));
}

}  // namespace cppref::files
