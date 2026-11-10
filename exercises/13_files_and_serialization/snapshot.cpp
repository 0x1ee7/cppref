// Engineer-owned. Contract: snapshot.hpp.
#include "snapshot.hpp"

#include <stdexcept>

namespace cppref::files {

std::vector<std::byte> encode_snapshot(std::uint32_t, Timestamp, std::span<const std::byte>, std::uint16_t) {
    throw std::logic_error("not implemented: encode_snapshot");
}

Snapshot decode_snapshot(std::span<const std::byte>) {
    throw std::logic_error("not implemented: decode_snapshot");
}

void write_snapshot(const std::filesystem::path&,
                    std::uint32_t,
                    Timestamp,
                    std::span<const std::byte>,
                    std::uint16_t) {
    throw std::logic_error("not implemented: write_snapshot");
}

Snapshot read_snapshot(const std::filesystem::path&) {
    throw std::logic_error("not implemented: read_snapshot");
}

}  // namespace cppref::files
