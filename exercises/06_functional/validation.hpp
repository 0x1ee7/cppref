#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

// Part 6 · Validator combinators (engineer-owned).
//
// Input is a dynamic JSON-like value (the request payload). A validator is a
// callable Value -> Errors; an empty Errors means "valid". Combinators collect
// messages and never throw on bad input. Primitives judge only their own domain
// and return no errors for other kinds (use is_type for type errors).
namespace cppref::functional {

struct Value;
using Array = std::vector<Value>;
using Object = std::vector<std::pair<std::string, Value>>;  // ordered key/value pairs

/// Null is std::monostate. Note that bool is a distinct alternative, so it is
/// never accepted where an integer is expected.
struct Value {
    std::variant<std::monostate, bool, std::int64_t, double, std::string, Array, Object> data;
};

enum class ValueKind { null, boolean, integer, real, string, array, object };

using Errors = std::vector<std::string>;
using Validator = std::function<Errors(const Value&)>;

/// Kind of a value.
[[nodiscard]] ValueKind kind_of(const Value& value);

/// Python-style name used in messages: "None", "bool", "int", "float", "str",
/// "list", "Mapping".
[[nodiscard]] std::string_view kind_name(ValueKind kind);

/// Errors: "expected <kind_name(expected)>, got <kind_name(actual)>".
[[nodiscard]] Validator is_type(ValueKind expected);

/// Strings only. Length is measured in bytes. Error: "length must be at least N".
[[nodiscard]] Validator min_length(std::size_t n);

/// Strings only, std::regex_match (whole string). Error: "must match pattern '<pattern>'".
[[nodiscard]] Validator matches(std::string pattern);

/// Integers and floats only, inclusive. Error: "must be between <lo> and <hi>".
[[nodiscard]] Validator in_range(double lo, double hi);

/// Runs every validator and concatenates all errors in order.
[[nodiscard]] Validator all_of(std::initializer_list<Validator> validators);

/// Returns no errors as soon as one validator passes, without calling the rest.
/// Otherwise returns every error concatenated. Throws std::invalid_argument when
/// given no validators (a programming error, not bad input).
[[nodiscard]] Validator any_of(std::initializer_list<Validator> validators);

/// Applies `validator` to the member `name` of an Object, prefixing errors with
/// "name: ". A missing member yields "name: missing". Any other kind yields no errors.
[[nodiscard]] Validator field(std::string name, Validator validator);

/// Null is valid; anything else is delegated.
[[nodiscard]] Validator optional(Validator validator);

/// Applies `validator` to every element of an Array, prefixing errors with "[i] ".
/// Other kinds yield no errors.
[[nodiscard]] Validator each(Validator validator);

/// Signup payload: must be an Object (else "expected Mapping, got <kind>"), with
///  username: str, at least 3 bytes, matching [a-z0-9_]+
///  age:      int, 13..120 inclusive
///  email:    str, matching [^@\s]+@[^@\s]+\.[^@\s]+
///  tags:     optional list of non-empty strings
/// Errors appear in that order.
[[nodiscard]] Errors validate_signup(const Value& payload);

}  // namespace cppref::functional
