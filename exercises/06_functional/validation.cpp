#include "validation.hpp"

#include <stdexcept>

// Part 6 · Validator combinators. Stub: implement every function.

namespace cppref::functional {

ValueKind kind_of(const Value&) {
    throw std::logic_error("not implemented: kind_of");
}

std::string_view kind_name(ValueKind) {
    throw std::logic_error("not implemented: kind_name");
}

Validator is_type(ValueKind) {
    throw std::logic_error("not implemented: is_type");
}

Validator min_length(std::size_t) {
    throw std::logic_error("not implemented: min_length");
}

Validator matches(std::string) {
    throw std::logic_error("not implemented: matches");
}

Validator in_range(double, double) {
    throw std::logic_error("not implemented: in_range");
}

Validator all_of(std::initializer_list<Validator>) {
    throw std::logic_error("not implemented: all_of");
}

Validator any_of(std::initializer_list<Validator>) {
    throw std::logic_error("not implemented: any_of");
}

Validator field(std::string, Validator) {
    throw std::logic_error("not implemented: field");
}

Validator optional(Validator) {
    throw std::logic_error("not implemented: optional");
}

Validator each(Validator) {
    throw std::logic_error("not implemented: each");
}

Errors validate_signup(const Value&) {
    throw std::logic_error("not implemented: validate_signup");
}

}  // namespace cppref::functional
