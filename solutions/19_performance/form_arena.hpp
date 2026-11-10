// Part 5: request-scoped parsing into a std::pmr::monotonic_buffer_resource arena.
#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <memory_resource>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cppref::performance {

// One decoded key/value pair. Holds pmr strings; construct with (allocator_arg, alloc, key, value)
// so both strings draw from the same arena. No allocator_type member: the vector constructs it
// directly rather than through uses-allocator dispatch.
struct FormField {

    FormField(std::allocator_arg_t, std::pmr::polymorphic_allocator<char> alloc,
              std::pmr::string key_text,
              std::pmr::string value_text)
        : key(std::move(key_text), alloc), value(std::move(value_text), alloc) {}
    FormField(const FormField&) = default;
    FormField(FormField&&) noexcept = default;
    FormField& operator=(const FormField&) = default;
    FormField& operator=(FormField&&) noexcept = default;
    ~FormField() = default;

    std::pmr::string key;
    std::pmr::string value;
};

enum class FormParseErrorKind : std::uint8_t {
    bad_escape,  // '%' not followed by two hex digits
    empty_key,   // a segment that starts with '=' (or is just "=")
};

struct FormParseError {
    FormParseErrorKind kind;
    std::size_t offset;  // byte offset in the body of the offending '%' or segment start
};

// Parses an application/x-www-form-urlencoded body: "k1=v1&k2=v2&flag".
//  - Pairs are split on '&'; empty segments are skipped.
//  - The first '=' splits key from value; a segment without '=' has an empty value.
//  - '+' decodes to a space; "%XX" (either hex case) decodes to one byte.
//  - Every key, value and the result vector itself is allocated from `arena` only.
// The returned vector keeps `arena` as its resource, also after it is moved.
std::expected<std::pmr::vector<FormField>, FormParseError> parse_form(
    std::string_view body, std::pmr::memory_resource* arena);

}  // namespace cppref::performance
