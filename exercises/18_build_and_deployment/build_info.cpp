#include "build_info.hpp"

#include <stdexcept>

namespace cppref::deployment {

std::string Version::to_string() const {
    throw std::logic_error("not implemented: Version::to_string");
}

std::expected<Version, std::string> parse_version(std::string_view) {
    throw std::logic_error("not implemented: parse_version");
}

bool is_git_sha(std::string_view) {
    throw std::logic_error("not implemented: is_git_sha");
}

std::string BuildInfo::version_line() const {
    throw std::logic_error("not implemented: BuildInfo::version_line");
}

BuildInfo build_info() {
    throw std::logic_error("not implemented: build_info");
}

}  // namespace cppref::deployment
