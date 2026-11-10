#include "static_locals.hpp"

#include <stdexcept>

namespace cppref::statics {

std::uint32_t crc32(std::string_view) {
    throw std::logic_error("not implemented: crc32");
}

std::size_t crc_table_build_count() {
    throw std::logic_error("not implemented: crc_table_build_count");
}

AuditLog& AuditLog::instance() {
    throw std::logic_error("not implemented: AuditLog::instance");
}

void AuditLog::append(std::string_view) {
    throw std::logic_error("not implemented: AuditLog::append");
}

std::vector<std::string> AuditLog::lines() const {
    throw std::logic_error("not implemented: AuditLog::lines");
}

void AuditLog::clear() {
    throw std::logic_error("not implemented: AuditLog::clear");
}

}  // namespace cppref::statics
