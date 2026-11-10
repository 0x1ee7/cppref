#include "digest.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace cppref::files {
namespace fs = std::filesystem;

namespace {

constexpr std::array<std::uint32_t, 64> round_constants = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

constexpr std::array<std::uint32_t, 8> initial_state = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
};

constexpr std::uint32_t rotr(std::uint32_t x, int n) noexcept {
    return (x >> n) | (x << (32 - n));
}

constexpr std::array<std::uint32_t, 256> make_crc_table() {
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t c = i;
        for (int k = 0; k < 8; ++k) {
            c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        table[i] = c;
    }
    return table;
}

constexpr auto crc_table = make_crc_table();

}  // namespace

Sha256::Sha256() : state_(initial_state) {}

void Sha256::compress(const std::uint8_t* block) {
    std::array<std::uint32_t, 64> w{};
    for (int i = 0; i < 16; ++i) {
        w[i] = (std::uint32_t{block[4 * i]} << 24) | (std::uint32_t{block[4 * i + 1]} << 16) |
               (std::uint32_t{block[4 * i + 2]} << 8) | std::uint32_t{block[4 * i + 3]};
    }
    for (int i = 16; i < 64; ++i) {
        const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    std::uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
    std::uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];
    for (int i = 0; i < 64; ++i) {
        const std::uint32_t big_s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        const std::uint32_t ch = (e & f) ^ (~e & g);
        const std::uint32_t t1 = h + big_s1 + ch + round_constants[i] + w[i];
        const std::uint32_t big_s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t t2 = big_s0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

void Sha256::absorb(const std::uint8_t* data, std::size_t size) {
    total_bytes_ += size;
    if (buffered_ > 0) {
        const std::size_t take = std::min(buffer_.size() - buffered_, size);
        std::memcpy(buffer_.data() + buffered_, data, take);
        buffered_ += take;
        data += take;
        size -= take;
        if (buffered_ == buffer_.size()) {
            compress(buffer_.data());
            buffered_ = 0;
        }
    }
    while (size >= buffer_.size()) {
        compress(data);
        data += buffer_.size();
        size -= buffer_.size();
    }
    if (size > 0) {
        std::memcpy(buffer_.data(), data, size);
        buffered_ = size;
    }
}

void Sha256::update(std::span<const std::byte> data) {
    if (finished_) {
        throw std::logic_error("Sha256::update called after finish()");
    }
    absorb(reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
}

void Sha256::update(std::string_view text) {
    if (finished_) {
        throw std::logic_error("Sha256::update called after finish()");
    }
    absorb(reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
}

Sha256Digest Sha256::finish() {
    if (finished_) {
        throw std::logic_error("Sha256::finish called twice");
    }
    finished_ = true;

    const std::uint64_t bit_length = total_bytes_ * 8;
    buffer_[buffered_++] = 0x80;
    if (buffered_ > 56) {
        std::fill(buffer_.begin() + static_cast<std::ptrdiff_t>(buffered_), buffer_.end(), std::uint8_t{0});
        compress(buffer_.data());
        buffered_ = 0;
    }
    std::fill(buffer_.begin() + static_cast<std::ptrdiff_t>(buffered_), buffer_.begin() + 56, std::uint8_t{0});
    for (int i = 0; i < 8; ++i) {
        buffer_[56 + i] = static_cast<std::uint8_t>(bit_length >> (56 - 8 * i));
    }
    compress(buffer_.data());

    Sha256Digest out{};
    for (std::size_t i = 0; i < 8; ++i) {
        out[4 * i] = static_cast<std::uint8_t>(state_[i] >> 24);
        out[4 * i + 1] = static_cast<std::uint8_t>(state_[i] >> 16);
        out[4 * i + 2] = static_cast<std::uint8_t>(state_[i] >> 8);
        out[4 * i + 3] = static_cast<std::uint8_t>(state_[i]);
    }
    return out;
}

std::string to_hex(std::span<const std::uint8_t> bytes) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (const std::uint8_t b : bytes) {
        out += digits[b >> 4];
        out += digits[b & 0x0F];
    }
    return out;
}

std::string sha256_hex(std::span<const std::byte> data) {
    Sha256 hasher;
    hasher.update(data);
    const Sha256Digest digest = hasher.finish();
    return to_hex(digest);
}

std::string sha256_hex(std::string_view text) {
    Sha256 hasher;
    hasher.update(text);
    const Sha256Digest digest = hasher.finish();
    return to_hex(digest);
}

std::string sha256_file(const fs::path& path, std::size_t chunk_size) {
    if (chunk_size == 0) {
        throw std::invalid_argument("sha256_file: chunk_size must be positive");
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw fs::filesystem_error("cannot open file", path,
                                   std::make_error_code(std::errc::no_such_file_or_directory));
    }
    Sha256 hasher;
    std::vector<char> buffer(chunk_size);
    while (in) {
        in.read(buffer.data(), static_cast<std::streamsize>(chunk_size));
        const auto got = in.gcount();
        if (got > 0) {
            hasher.update(std::string_view(buffer.data(), static_cast<std::size_t>(got)));
        }
    }
    if (in.bad()) {
        throw fs::filesystem_error("read failed", path, std::make_error_code(std::errc::io_error));
    }
    const Sha256Digest digest = hasher.finish();
    return to_hex(digest);
}

std::uint32_t crc32(std::span<const std::byte> data, std::uint32_t seed) {
    std::uint32_t c = seed ^ 0xFFFFFFFFu;
    for (const std::byte b : data) {
        c = crc_table[(c ^ std::to_integer<std::uint8_t>(b)) & 0xFFu] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

}  // namespace cppref::files
