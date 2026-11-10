// Engineer-owned. Contract: scan.hpp.
#include "scan.hpp"

#include <stdexcept>

namespace cppref::files {

bool glob_match(std::string_view, std::string_view) {
    throw std::logic_error("not implemented: glob_match");
}

bool is_ignored(std::string_view, const std::vector<std::string>&) {
    throw std::logic_error("not implemented: is_ignored");
}

Manifest scan_tree(const std::filesystem::path&, const std::vector<std::string>&) {
    throw std::logic_error("not implemented: scan_tree");
}

}  // namespace cppref::files
