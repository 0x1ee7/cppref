#include "temp_dir.hpp"

#include <stdexcept>

namespace cppref::raii_and_memory {

TempDir::TempDir(std::string_view prefix) {
    (void)prefix;
    throw std::logic_error("not implemented: TempDir::TempDir");
}

std::filesystem::path TempDir::file(std::string_view name) const {
    (void)name;
    throw std::logic_error("not implemented: TempDir::file");
}

}  // namespace cppref::raii_and_memory
