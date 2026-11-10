// Engineer-owned. Contract: digest.hpp.
#include "digest.hpp"

#include <stdexcept>

namespace cppref::files {

Sha256::Sha256() { throw std::logic_error("not implemented: Sha256::Sha256"); }

void Sha256::update(std::span<const std::byte>) {
    throw std::logic_error("not implemented: Sha256::update(bytes)");
}

void Sha256::update(std::string_view) {
    throw std::logic_error("not implemented: Sha256::update(text)");
}

Sha256Digest Sha256::finish() { throw std::logic_error("not implemented: Sha256::finish"); }

std::string to_hex(std::span<const std::uint8_t>) {
    throw std::logic_error("not implemented: to_hex");
}

std::string sha256_hex(std::span<const std::byte>) {
    throw std::logic_error("not implemented: sha256_hex(bytes)");
}

std::string sha256_hex(std::string_view) {
    throw std::logic_error("not implemented: sha256_hex(text)");
}

std::string sha256_file(const std::filesystem::path&, std::size_t) {
    throw std::logic_error("not implemented: sha256_file");
}

std::uint32_t crc32(std::span<const std::byte>, std::uint32_t) {
    throw std::logic_error("not implemented: crc32");
}

}  // namespace cppref::files
