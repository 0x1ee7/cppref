// Engineer-owned. Contract: sync.hpp.
#include "sync.hpp"

#include <stdexcept>

namespace cppref::files {

bool ManifestDiff::empty() const noexcept { return added.empty() && removed.empty() && modified.empty(); }

ManifestDiff diff_manifests(const Manifest&, const Manifest&) {
    throw std::logic_error("not implemented: diff_manifests");
}

void validate_relative_key(std::string_view) {
    throw std::logic_error("not implemented: validate_relative_key");
}

ApplyResult apply_diff(const ManifestDiff&, const std::filesystem::path&, const std::filesystem::path&) {
    throw std::logic_error("not implemented: apply_diff");
}

}  // namespace cppref::files
