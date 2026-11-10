#include "formula.hpp"

#include <stdexcept>

namespace cppref::formula {

void Environment::set_cell(std::string, double) {
    throw std::logic_error("not implemented: Environment::set_cell");
}

void Environment::define_function(std::string, std::optional<std::size_t>, Function) {
    throw std::logic_error("not implemented: Environment::define_function");
}

std::optional<double> Environment::cell(std::string_view) const {
    throw std::logic_error("not implemented: Environment::cell");
}

std::optional<Environment::FunctionRef> Environment::find_function(std::string_view) const {
    throw std::logic_error("not implemented: Environment::find_function");
}

Environment standard_environment() {
    throw std::logic_error("not implemented: standard_environment");
}

std::expected<double, Error> evaluate(std::string_view, const Environment&) {
    throw std::logic_error("not implemented: evaluate");
}

std::expected<std::vector<std::string>, Error> referenced_cells(std::string_view) {
    throw std::logic_error("not implemented: referenced_cells");
}

}  // namespace cppref::formula
