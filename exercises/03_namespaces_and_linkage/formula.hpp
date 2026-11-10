#pragma once

#include <cstddef>
#include <expected>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Part 7 — a small spreadsheet-formula evaluator: explicit environments, separate namespaces
// for cells and functions, and error values via std::expected.
namespace cppref::formula {

enum class ErrorKind { syntax, unknown_cell, unknown_function, arity, division_by_zero };

struct Error {
    ErrorKind kind;
    std::string message;

    friend bool operator==(const Error&, const Error&) = default;
};

class Environment {
public:
    using Function = std::function<double(std::span<const double>)>;

    // A function together with its arity. Nothing else may refer to the internal tables.
    struct FunctionRef {
        std::optional<std::size_t> arity;  // nullopt = variadic
        const Function& impl;
    };

    // Sets (or replaces) the value of a cell.
    void set_cell(std::string name, double value);

    // Defines (or replaces) a function. arity == std::nullopt means variadic, including zero args.
    void define_function(std::string name, std::optional<std::size_t> arity, Function impl);

    // The value of a cell, or std::nullopt if unset.
    std::optional<double> cell(std::string_view name) const;

    // The function named `name`, or std::nullopt if undefined.
    std::optional<FunctionRef> find_function(std::string_view name) const;

private:
    // Engineer: two separate tables — cells and functions live in different namespaces.
};

// Built-in functions: SUM (variadic), ABS (1 arg), MIN (2 args), MAX (2 args). No cells set.
Environment standard_environment();

// Evaluates an expression against env.
//
// Grammar:
//   expr    := term   (('+' | '-') term)*
//   term    := unary  (('*' | '/') unary)*
//   unary   := '-' unary | primary
//   primary := NUMBER | IDENT '(' [expr (',' expr)*] ')' | IDENT | '(' expr ')'
//
// IDENT: a letter or '_', then letters, digits or '_'. An IDENT followed by '(' is a function
// call; otherwise it is a cell reference. Whitespace is allowed between tokens. NUMBER is
// locale-independent (use std::from_chars).
//
// Errors: syntax; unknown_cell; unknown_function; arity; division_by_zero.
std::expected<double, Error> evaluate(std::string_view expression, const Environment& env);

// The distinct cell identifiers in `expression`, sorted ascending. Function names are not
// cells. Syntax errors are reported as in evaluate().
std::expected<std::vector<std::string>, Error> referenced_cells(std::string_view expression);

}  // namespace cppref::formula
