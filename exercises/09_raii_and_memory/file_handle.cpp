#include "unique_handle.hpp"

#include <stdexcept>

namespace cppref::raii_and_memory {

std::expected<FileHandle, std::error_code> open_file(const std::filesystem::path& path, const char* mode) {
    (void)path;
    (void)mode;
    throw std::logic_error("not implemented: open_file");
}

}  // namespace cppref::raii_and_memory
