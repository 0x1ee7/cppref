#include "version.hpp"

#include <ostream>
#include <stdexcept>
#include <string>

namespace cppref::classes {
namespace {

[[noreturn]] void not_implemented(const char* name) {
    throw std::logic_error(std::string("not implemented: ") + name);
}

}  // namespace

std::expected<Version, VersionError> Version::create(int, int, int, std::string_view, std::string_view) {
    not_implemented("Version::create");
}

std::expected<Version, VersionError> Version::parse(std::string_view) {
    not_implemented("Version::parse");
}

int Version::major() const {
    not_implemented("Version::major");
}

int Version::minor() const {
    not_implemented("Version::minor");
}

int Version::patch() const {
    not_implemented("Version::patch");
}

std::string_view Version::prerelease() const {
    not_implemented("Version::prerelease");
}

std::string_view Version::build() const {
    not_implemented("Version::build");
}

std::expected<Version, VersionError> Version::bump(BumpPart) const {
    not_implemented("Version::bump");
}

std::string Version::to_string() const {
    not_implemented("Version::to_string");
}

bool operator==(const Version&, const Version&) {
    not_implemented("operator==(Version, Version)");
}

std::strong_ordering operator<=>(const Version&, const Version&) {
    not_implemented("operator<=>(Version, Version)");
}

std::ostream& operator<<(std::ostream&, const Version&) {
    not_implemented("operator<<(ostream, Version)");
}

}  // namespace cppref::classes

std::size_t std::hash<cppref::classes::Version>::operator()(const cppref::classes::Version&) const {
    throw std::logic_error("not implemented: std::hash<Version>::operator()");
}
