#include "unique_handle.hpp"

#include <cerrno>
#include <cstdio>
#include <expected>
#include <system_error>

namespace cppref::raii_and_memory {

std::expected<FileHandle, std::error_code> open_file(const std::filesystem::path& path, const char* mode) {
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)  // std::fopen is deprecated only by MSVC; kept for portability
#endif
    std::FILE* f = std::fopen(path.string().c_str(), mode);
#ifdef _MSC_VER
#pragma warning(pop)
#endif
    if (f == nullptr) return std::unexpected(std::error_code(errno, std::generic_category()));
    return FileHandle(f);
}

}  // namespace cppref::raii_and_memory
