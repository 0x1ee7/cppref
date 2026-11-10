// Minimal JSON value, writer and strict reader for the manifest subset.
#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace cppref::files {

// Thrown for parse errors (with the byte offset of the offending character) and for
// type/shape errors (offset() == npos).
class JsonError : public std::runtime_error {
public:
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    explicit JsonError(const std::string& message, std::size_t offset = npos)
        : std::runtime_error(message), offset_(offset) {}

    [[nodiscard]] std::size_t offset() const noexcept { return offset_; }

private:
    std::size_t offset_;
};

// JSON restricted to the manifest subset: null, bool, 64-bit integer, string, array, object.
// There are no floating-point numbers: parse() rejects them.
// Object members keep their insertion (or parse) order in as_object(); dump() emits keys
// sorted bytewise, so output is deterministic.
class Json {
public:
    enum class Kind { null, boolean, integer, string, array, object };
    using Array = std::vector<Json>;
    // Vector of pairs keeps Json usable while still incomplete inside its own definition.
    using Object = std::vector<std::pair<std::string, Json>>;

    static Json null();
    static Json boolean(bool value);
    static Json integer(std::int64_t value);
    static Json string(std::string value);
    static Json array(Array items);
    // Duplicate keys -> JsonError.
    static Json object(Object members);

    [[nodiscard]] Kind kind() const noexcept;

    // Accessors throw JsonError (offset() == npos) on a kind mismatch.
    [[nodiscard]] bool as_bool() const;
    [[nodiscard]] std::int64_t as_integer() const;
    [[nodiscard]] const std::string& as_string() const;
    [[nodiscard]] const Array& as_array() const;
    [[nodiscard]] const Object& as_object() const;

    // Member lookup: pointer to the value, or nullptr when absent. JsonError if not an object.
    [[nodiscard]] const Json* find(std::string_view key) const;

    // Compact when indent < 0. Otherwise one member/element per line, `indent` spaces per level;
    // empty containers print as [] and {}. Strings: '"' and '\\' escaped; \b \f \n \r \t use
    // short forms; other control characters < 0x20 become \u00xx (lowercase hex). Bytes
    // >= 0x80 are emitted as-is and must form valid UTF-8, else JsonError.
    [[nodiscard]] std::string dump(int indent = -1) const;

    // Strict RFC 8259 parse of the subset. Rejects: empty input, trailing content, fractions,
    // exponents, leading zeros, integers outside int64, raw control characters in strings,
    // invalid escapes, lone UTF-16 surrogates, duplicate object keys, invalid UTF-8 in
    // strings, nesting deeper than 128 levels (128 is allowed). \uXXXX escapes (including
    // surrogate pairs) decode to UTF-8. The offset of a parse error is the byte index of the
    // offending character, or the input size at unexpected end of input.
    [[nodiscard]] static Json parse(std::string_view text);

private:
    using Storage = std::variant<std::nullptr_t, bool, std::int64_t, std::string, Array, Object>;

    explicit Json(Storage value) : value_(std::move(value)) {}
    void write(std::string& out, int indent, int depth) const;

    Storage value_;
};

}  // namespace cppref::files
