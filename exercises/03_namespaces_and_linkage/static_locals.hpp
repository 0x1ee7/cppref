#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Part 3 — function-local statics: lazy initialisation, thread-safe init and the
// Meyers singleton.
namespace cppref::statics {

// CRC-32 (IEEE 802.3, reflected, polynomial 0xEDB88320, init and final XOR 0xFFFFFFFF).
// crc32("123456789") == 0xCBF43926 and crc32("") == 0.
// The 256-entry table lives in a function-local static, built on first use.
std::uint32_t crc32(std::string_view data);

// Number of times the CRC table has been built in this process. Must be 0 before the
// first crc32 call and 1 after it, even when first use races across threads.
std::size_t crc_table_build_count();

// Meyers singleton: an append-only, thread-safe audit trail.
class AuditLog {
public:
    // Returns the process-wide instance, constructed on first use.
    static AuditLog& instance();

    AuditLog(const AuditLog&) = delete;
    AuditLog& operator=(const AuditLog&) = delete;

    // Thread-safe.
    void append(std::string_view line);

    // Thread-safe snapshot copy of the lines in append order.
    std::vector<std::string> lines() const;

    // Thread-safe.
    void clear();

private:
    AuditLog() = default;
    // Engineer: storage plus a mutex (mutable, since lines() is const).
};

}  // namespace cppref::statics
