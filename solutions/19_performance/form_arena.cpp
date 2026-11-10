#include "form_arena.hpp"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <memory_resource>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cppref::performance {
namespace {

using Decoded = std::expected<std::pmr::string, FormParseError>;
using Result = std::expected<std::pmr::vector<FormField>, FormParseError>;

int hex_value(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Decodes '+' and %XX in `raw`. `base` is the body offset of raw[0], used for error offsets.
Decoded decode(std::string_view raw, std::size_t base, std::pmr::memory_resource* arena) {
    std::pmr::string out(std::pmr::polymorphic_allocator<char>{arena});
    out.reserve(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        const char c = raw[i];
        if (c == '+') {
            out.push_back(' ');
        } else if (c == '%') {
            if (i + 2 >= raw.size()) {
                return std::unexpected(FormParseError{FormParseErrorKind::bad_escape, base + i});
            }
            const int hi = hex_value(raw[i + 1]);
            const int lo = hex_value(raw[i + 2]);
            if (hi < 0 || lo < 0) {
                return std::unexpected(FormParseError{FormParseErrorKind::bad_escape, base + i});
            }
            out.push_back(static_cast<char>(hi * 16 + lo));
            i += 2;
        } else {
            out.push_back(c);
        }
    }
    return Decoded(std::move(out));
}

}  // namespace

Result parse_form(std::string_view body, std::pmr::memory_resource* arena) {
    std::pmr::vector<FormField> fields(std::pmr::polymorphic_allocator<FormField>{arena});
    // One reserve for the upper bound on pair count ('&' count + 1). Regrowth would leave every
    // superseded block behind in a monotonic arena, which is what exhausts it.
    if (!body.empty()) {
        fields.reserve(static_cast<std::size_t>(std::count(body.begin(), body.end(), '&')) + 1);
    }

    std::size_t pos = 0;
    while (pos < body.size()) {
        const std::size_t amp = body.find('&', pos);
        const std::size_t end = amp == std::string_view::npos ? body.size() : amp;
        const std::size_t segment_start = pos;
        pos = amp == std::string_view::npos ? body.size() : amp + 1;

        const std::string_view segment = body.substr(segment_start, end - segment_start);
        if (segment.empty()) continue;

        const std::size_t eq = segment.find('=');
        const std::string_view raw_key = segment.substr(0, eq);
        if (raw_key.empty()) {
            return std::unexpected(FormParseError{FormParseErrorKind::empty_key, segment_start});
        }
        const std::string_view raw_value =
            eq == std::string_view::npos ? std::string_view{} : segment.substr(eq + 1);
        const std::size_t value_base = segment_start + (eq == std::string_view::npos ? 0 : eq + 1);

        auto key = decode(raw_key, segment_start, arena);
        if (!key) return std::unexpected(key.error());
        auto value = decode(raw_value, value_base, arena);
        if (!value) return std::unexpected(value.error());

        fields.emplace_back(std::allocator_arg, std::pmr::polymorphic_allocator<char>{arena},
                            std::move(*key), std::move(*value));
    }
    return Result(std::move(fields));
}

}  // namespace cppref::performance
