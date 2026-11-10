#include "linkage.hpp"

#include <stdexcept>

namespace cppref::linkage {

std::string audit_line(std::string_view, std::string_view) {
    throw std::logic_error("not implemented: audit_line");
}

ErrorCatalog::ErrorCatalog() {
    throw std::logic_error("not implemented: ErrorCatalog::ErrorCatalog");
}

std::optional<std::string_view> ErrorCatalog::find(int) const {
    throw std::logic_error("not implemented: ErrorCatalog::find");
}

std::size_t ErrorCatalog::size() const {
    throw std::logic_error("not implemented: ErrorCatalog::size");
}

const ErrorCatalog& error_catalog() {
    throw std::logic_error("not implemented: error_catalog");
}

}  // namespace cppref::linkage
