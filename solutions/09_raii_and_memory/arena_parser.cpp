#include "arena_parser.hpp"

#include <cstddef>
#include <expected>
#include <memory_resource>
#include <string_view>
#include <utility>

namespace cppref::raii_and_memory {

namespace {

bool is_space(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r';
}

}  // namespace

std::expected<std::pmr::vector<Token>, ParseError> tokenize(std::string_view input,
                                                            std::pmr::memory_resource* arena) {
    std::pmr::vector<Token> out(arena);
    std::size_t line = 1;
    std::size_t i = 0;
    while (i < input.size()) {
        const char c = input[i];
        if (c == '\n') {
            ++line;
            ++i;
        } else if (is_space(c)) {
            ++i;
        } else if (c == '"') {
            std::size_t j = i + 1;
            while (j < input.size() && input[j] != '"') {
                if (input[j] == '\n') return std::unexpected(ParseError{line, "newline inside quoted token"});
                ++j;
            }
            if (j == input.size()) return std::unexpected(ParseError{line, "unterminated quote"});
            out.emplace_back(input.substr(i + 1, j - i - 1), line);
            i = j + 1;
        } else {
            std::size_t j = i;
            while (j < input.size() && !is_space(input[j]) && input[j] != '\n') ++j;
            out.emplace_back(input.substr(i, j - i), line);
            i = j;
        }
    }
    return std::expected<std::pmr::vector<Token>, ParseError>(std::move(out));
}

}  // namespace cppref::raii_and_memory
