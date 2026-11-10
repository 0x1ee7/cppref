#pragma once

#include <cstddef>
#include <memory>
#include <expected>
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

namespace cppref::raii_and_memory {

// A token whose text lives in a caller-supplied memory_resource.
// Allocator-aware: std::pmr::vector<Token> passes its allocator to Token through
// uses-allocator construction (allocator_arg_t form), so emplace_back(text, line) lands the
// string in the vector's arena. Without allocator_type, the string would use the default resource.
struct Token {
    using allocator_type = std::pmr::polymorphic_allocator<char>;

    Token(std::allocator_arg_t, allocator_type alloc, std::string_view text, std::size_t line)
        : text(text, alloc), line(line) {}
    // Copy and move keep the source's resource (polymorphic_allocator semantics).
    Token(const Token&) = default;
    Token(Token&&) noexcept = default;
    Token& operator=(const Token&) = default;
    Token& operator=(Token&&) noexcept = default;
    ~Token() = default;

    std::pmr::string text;
    std::size_t line;
};

struct ParseError {
    std::size_t line;
    std::string message;
};

// Splits input into tokens separated by spaces, tabs and newlines. Double quotes group text
// that contains spaces; the quotes are removed and "" inside quotes is not special. A newline
// inside quotes is an error. Line numbers start at 1.
// Returns ParseError{line, "unterminated quote"} for an open quote at end of input.
// All token storage (vector and strings) comes from arena; a null arena is a programming error.
std::expected<std::pmr::vector<Token>, ParseError> tokenize(std::string_view input,
                                                            std::pmr::memory_resource* arena);

}  // namespace cppref::raii_and_memory
