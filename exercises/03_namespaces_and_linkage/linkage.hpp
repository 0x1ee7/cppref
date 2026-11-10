#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

// Part 2 — internal vs external linkage, inline functions and variables, and
// construct-on-first-use globals.
namespace cppref::linkage {

// Inline variable: one object across all translation units (C++17).
inline constexpr std::size_t kMaxAuditLine = 120;

// Inline function with a function-local static. The header is the definition, so every
// TU that includes it refers to the same counter. Returns the post-increment value.
inline int hit_counter() {
    throw std::logic_error("not implemented: hit_counter");
}

// "[component] message". If that exceeds kMaxAuditLine characters, the result is cut to
// kMaxAuditLine characters with the last three replaced by "...". A line of exactly
// kMaxAuditLine characters is not truncated.
std::string audit_line(std::string_view component, std::string_view message);

// Immutable code->message table. Constructed once via error_catalog().
// Engineer-owned private members (e.g. std::map<int, std::string> entries_).
class ErrorCatalog {
public:
    // Must contain exactly: 400 "Bad Request", 404 "Not Found", 500 "Internal Server Error".
    ErrorCatalog();

    // The message for code, or std::nullopt if unknown. The view remains valid as long as
    // this catalog exists.
    std::optional<std::string_view> find(int code) const;

    std::size_t size() const;

private:
    // Engineer: storage goes here.
};

// Process-wide catalog built on first use (construct-on-first-use, thread-safe).
const ErrorCatalog& error_catalog();

}  // namespace cppref::linkage
