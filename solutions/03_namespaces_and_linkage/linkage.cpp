#include "linkage.hpp"

#include <format>
#include <string>
#include <string_view>

namespace cppref::linkage {

std::string audit_line(std::string_view component, std::string_view message) {
    std::string line = std::format("[{}] {}", component, message);
    if (line.size() > kMaxAuditLine) {
        line.resize(kMaxAuditLine);
        line.replace(kMaxAuditLine - 3, 3, "...");
    }
    return line;
}

ErrorCatalog::ErrorCatalog()
    : entries_{{400, "Bad Request"}, {404, "Not Found"}, {500, "Internal Server Error"}} {}

std::optional<std::string_view> ErrorCatalog::find(int code) const {
    const auto it = entries_.find(code);
    if (it == entries_.end()) return std::nullopt;
    return std::string_view(it->second);
}

std::size_t ErrorCatalog::size() const noexcept {
    return entries_.size();
}

const ErrorCatalog& error_catalog() {
    static const ErrorCatalog catalog;
    return catalog;
}

}  // namespace cppref::linkage
