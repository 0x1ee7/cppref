#pragma once

#include <cstddef>
#include <expected>
#include <memory>
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace cppref::raii_and_memory {

// A token whose text lives in the arena. Allocator-aware: std::pmr::vector<Token> passes its
// allocator through uses-allocator construction (allocator_arg_t form), so emplace_back(text, line)
// places the string in the vector's arena.
struct Token {
    using allocator_type = std::pmr::polymorphic_allocator<char>;

    Token(std::allocator_arg_t, allocator_type alloc, std::string_view value, std::size_t line_no)
        : text(value, alloc), line(line_no) {}
    // Allocator-extended copy/move: needed because vector reallocation move-constructs elements
    // through uses-allocator construction, which requires these exact forms.
    Token(std::allocator_arg_t, allocator_type alloc, const Token& other)
        : text(other.text, alloc), line(other.line) {}
    Token(std::allocator_arg_t, allocator_type alloc, Token&& other)
        : text(std::move(other.text), alloc), line(other.line) {}
    Token(const Token&) = default;
    Token(Token&&) noexcept = default;
    Token& operator=(const Token&) = default;
    Token& operator=(Token&&) = default;
    ~Token() = default;

    std::pmr::string text;
    std::size_t line;
};

struct ParseError {
    std::size_t line;
    std::string message;
};

// Tokenizes input; see the README for the grammar. All storage comes from arena.
std::expected<std::pmr::vector<Token>, ParseError> tokenize(std::string_view input,
                                                            std::pmr::memory_resource* arena);

}  // namespace cppref::raii_and_memory
