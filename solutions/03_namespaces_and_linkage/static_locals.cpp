#include "static_locals.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace cppref::statics {

namespace {

struct CrcTable {
    std::array<std::uint32_t, 256> entries{};
};

std::atomic<std::size_t> g_table_builds{0};

// Built exactly once: the C++11 magic-static guarantee serialises racing first callers.
const CrcTable& crc_table() {
    static const CrcTable table = [] {
        g_table_builds.fetch_add(1, std::memory_order_relaxed);
        CrcTable t;
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            t.entries[i] = c;
        }
        return t;
    }();
    return table;
}

}  // namespace

std::uint32_t crc32(std::string_view data) {
    const auto& table = crc_table().entries;
    std::uint32_t crc = 0xFFFFFFFFu;
    for (const unsigned char byte : data) {
        crc = table[(crc ^ byte) & 0xFFu] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

std::size_t crc_table_build_count() {
    return g_table_builds.load(std::memory_order_relaxed);
}

AuditLog& AuditLog::instance() {
    static AuditLog log;
    return log;
}

void AuditLog::append(std::string_view line) {
    std::lock_guard lock(mutex_);
    lines_.emplace_back(line);
}

std::vector<std::string> AuditLog::lines() const {
    std::lock_guard lock(mutex_);
    return lines_;
}

void AuditLog::clear() {
    std::lock_guard lock(mutex_);
    lines_.clear();
}

}  // namespace cppref::statics
