#include "arena_parser.hpp"

#include <stdexcept>

namespace cppref::raii_and_memory {

std::expected<std::pmr::vector<Token>, ParseError> tokenize(std::string_view input,
                                                            std::pmr::memory_resource* arena) {
    (void)input;
    (void)arena;
    throw std::logic_error("not implemented: tokenize");
}

}  // namespace cppref::raii_and_memory
