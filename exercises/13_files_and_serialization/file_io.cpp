// Engineer-owned. Contract: file_io.hpp.
#include "file_io.hpp"

#include <stdexcept>

namespace cppref::files {

std::string read_file_binary(const std::filesystem::path&) {
    throw std::logic_error("not implemented: read_file_binary");
}

void write_file_atomic(const std::filesystem::path&, std::string_view) {
    throw std::logic_error("not implemented: write_file_atomic");
}

}  // namespace cppref::files
