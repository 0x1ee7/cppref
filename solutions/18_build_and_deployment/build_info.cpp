#include "build_info.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <system_error>

namespace cppref::deployment {

std::string Version::to_string() const {
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

std::expected<Version, std::string> parse_version(std::string_view text) {
    auto fail = [&] {
        return std::unexpected("expected MAJOR.MINOR.PATCH, got '" + std::string(text) + "'");
    };

    std::array<unsigned, 3> parts{};
    std::size_t pos = 0;
    for (std::size_t idx = 0; idx < 3; ++idx) {
        const auto dot = text.find('.', pos);
        if (idx < 2 && dot == std::string_view::npos) return fail();
        const auto end = idx < 2 ? dot : text.size();
        const auto field = text.substr(pos, end - pos);
        if (field.empty() || (field.size() > 1 && field.front() == '0')) return fail();

        unsigned value = 0;
        const auto [ptr, ec] = std::from_chars(field.data(), field.data() + field.size(), value);
        if (ec != std::errc{} || ptr != field.data() + field.size()) return fail();
        parts[idx] = value;
        pos = end + 1;
    }
    return Version{parts[0], parts[1], parts[2]};
}

bool is_git_sha(std::string_view text) {
    if (text.size() < 7 || text.size() > 40) return false;
    return std::ranges::all_of(text, [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

std::string BuildInfo::version_line() const {
    return "cppref-service " + version + " (" + git_sha + ")";
}

BuildInfo build_info() {
    return BuildInfo{CPPREF_BUILD_VERSION, CPPREF_BUILD_GIT_SHA};
}

}  // namespace cppref::deployment
