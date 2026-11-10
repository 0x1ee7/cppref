// Engineer-owned. Contract: manifest_store.hpp.
#include "manifest_store.hpp"

#include <stdexcept>

namespace cppref::files {

std::string manifest_to_json(const Manifest&) {
    throw std::logic_error("not implemented: manifest_to_json");
}

Manifest manifest_from_json(std::string_view) {
    throw std::logic_error("not implemented: manifest_from_json");
}

void save_manifest_atomic(const Manifest&, const std::filesystem::path&) {
    throw std::logic_error("not implemented: save_manifest_atomic");
}

Manifest load_manifest(const std::filesystem::path&) {
    throw std::logic_error("not implemented: load_manifest");
}

}  // namespace cppref::files
