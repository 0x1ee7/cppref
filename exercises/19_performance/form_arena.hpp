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

// One decoded key/value pair. Uses-allocator aware: a pmr::vector<FormField> hands its allocator
// to the strings through the (allocator_arg_t, alloc, ...) constructor.
struct FormField {
    using allocator_type = std::pmr::polymorphic_allocator<char>;

    FormField(std::allocator_arg_t, allocator_type /*alloc*/, std::pmr::string /*key_text*/,
              std::pmr::string /*value_text*/) {
        throw std::logic_error("not implemented: FormField(allocator_arg_t, ...)");
    }
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
//  - Every key, value and the result vector itself is allocated from `arena` only. Nothing is
//    allocated from the global heap while the arena has room. Exhaustion of an arena whose
//    upstream is null throws std::bad_alloc.
// The returned vector keeps `arena` as its resource, also after it is moved.
std::expected<std::pmr::vector<FormField>, FormParseError> parse_form(
    std::string_view body, std::pmr::memory_resource* arena);

}  // namespace cppref::performance
