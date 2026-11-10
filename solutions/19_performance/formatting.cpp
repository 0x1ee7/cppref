#include "formatting.hpp"

#include <cstddef>
#include <expected>
#include <format>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cppref::performance {

std::string join_csv(std::span<const std::string_view> fields) {
    std::string out;  // one named return on every path, so NRVO applies (no proxy-copy in Debug)
    if (fields.empty()) return out;

    std::size_t total = fields.size() - 1;  // separators
    for (const std::string_view field : fields) {
        if (field.find_first_of(",\n") != std::string_view::npos) {
            throw std::invalid_argument("join_csv: field contains ',' or '\\n'");
        }
        total += field.size();
    }

    out.reserve(total);
    bool first = true;
    for (const std::string_view field : fields) {
        if (!first) out.push_back(',');
        first = false;
        out.append(field);
    }
    return out;
}

std::string format_access_line(const AccessEntry& entry) {
    const std::size_t size =
        std::formatted_size("{} {} {} {}us", entry.method, entry.path, entry.status, entry.latency_us);
    std::string out;
    out.reserve(size);
    std::format_to(std::back_inserter(out), "{} {} {} {}us", entry.method, entry.path, entry.status,
                   entry.latency_us);
    return out;
}

std::expected<std::string_view, std::size_t> format_access_line_into(std::span<char> out,
                                                                     const AccessEntry& entry) {
    const auto result = std::format_to_n(out.data(), out.size(), "{} {} {} {}us", entry.method,
                                         entry.path, entry.status, entry.latency_us);
    const auto needed = static_cast<std::size_t>(result.size);
    if (needed > out.size()) return std::unexpected(needed);
    return std::string_view(out.data(), needed);
}

}  // namespace cppref::performance
