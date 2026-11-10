#include "json.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cppref::files {

namespace {

constexpr int max_depth = 128;

bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }

// Length of the valid UTF-8 sequence starting at text[pos] (1-4), or 0 if invalid.
// Rejects overlong forms, UTF-16 surrogates and code points above U+10FFFF.
std::size_t utf8_sequence_length(std::string_view text, std::size_t pos) {
    const auto lead = static_cast<unsigned char>(text[pos]);
    const auto cont = [&](std::size_t k) {
        return pos + k < text.size() && (static_cast<unsigned char>(text[pos + k]) & 0xC0) == 0x80;
    };
    const auto second = [&]() { return pos + 1 < text.size() ? static_cast<unsigned char>(text[pos + 1]) : 0u; };

    if (lead < 0x80) {
        return 1;
    }
    if (lead >= 0xC2 && lead <= 0xDF) {
        return cont(1) ? 2 : 0;
    }
    if (lead >= 0xE0 && lead <= 0xEF) {
        if (!cont(1) || !cont(2)) {
            return 0;
        }
        if (lead == 0xE0 && second() < 0xA0) {
            return 0;  // overlong
        }
        if (lead == 0xED && second() >= 0xA0) {
            return 0;  // surrogate
        }
        return 3;
    }
    if (lead >= 0xF0 && lead <= 0xF4) {
        if (!cont(1) || !cont(2) || !cont(3)) {
            return 0;
        }
        if (lead == 0xF0 && second() < 0x90) {
            return 0;  // overlong
        }
        if (lead == 0xF4 && second() >= 0x90) {
            return 0;  // above U+10FFFF
        }
        return 4;
    }
    return 0;
}

void append_utf8(std::string& out, std::uint32_t cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

void dump_string(std::string& out, std::string_view s) {
    constexpr char hex[] = "0123456789abcdef";
    out += '"';
    std::size_t i = 0;
    while (i < s.size()) {
        const auto c = static_cast<unsigned char>(s[i]);
        if (c >= 0x80) {
            const std::size_t len = utf8_sequence_length(s, i);
            if (len == 0) {
                throw JsonError("invalid UTF-8 in string");
            }
            out.append(s.substr(i, len));
            i += len;
            continue;
        }
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    out += "\\u00";
                    out += hex[c >> 4];
                    out += hex[c & 0x0F];
                } else {
                    out += static_cast<char>(c);
                }
        }
        ++i;
    }
    out += '"';
}

void newline_indent(std::string& out, int indent, int depth) {
    if (indent < 0) {
        return;
    }
    out += '\n';
    out.append(static_cast<std::size_t>(indent) * static_cast<std::size_t>(depth), ' ');
}

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    Json parse_document() {
        skip_ws();
        Json value = parse_value();
        skip_ws();
        if (pos_ != text_.size()) {
            fail("trailing content after value");
        }
        return value;
    }

private:
    [[noreturn]] void fail(const char* message) const { fail_at(message, pos_); }

    [[noreturn]] void fail_at(const char* message, std::size_t at) const {
        throw JsonError(std::string(message) + " at byte " + std::to_string(at), at);
    }

    void skip_ws() {
        while (pos_ < text_.size()) {
            const char c = text_[pos_];
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                break;
            }
            ++pos_;
        }
    }

    char peek() const {
        if (pos_ >= text_.size()) {
            fail("unexpected end of input");
        }
        return text_[pos_];
    }

    void enter() {
        if (++depth_ > max_depth) {
            fail("nesting too deep");
        }
    }
    void leave() noexcept { --depth_; }

    Json parse_value() {
        const char c = peek();
        switch (c) {
            case '{': return parse_object();
            case '[': return parse_array();
            case '"': return Json::string(parse_string());
            case 't': expect_literal("true"); return Json::boolean(true);
            case 'f': expect_literal("false"); return Json::boolean(false);
            case 'n': expect_literal("null"); return Json::null();
            default:
                if (c == '-' || is_digit(c)) {
                    return parse_number();
                }
                fail("unexpected character");
        }
    }

    void expect_literal(std::string_view literal) {
        if (text_.substr(pos_, literal.size()) != literal) {
            fail("invalid literal");
        }
        pos_ += literal.size();
    }

    Json parse_number() {
        const std::size_t start = pos_;
        if (text_[pos_] == '-') {
            ++pos_;
        }
        if (pos_ >= text_.size() || !is_digit(text_[pos_])) {
            fail("expected digit");
        }
        if (text_[pos_] == '0') {
            ++pos_;
            if (pos_ < text_.size() && is_digit(text_[pos_])) {
                fail("leading zeros are not allowed");
            }
        } else {
            while (pos_ < text_.size() && is_digit(text_[pos_])) {
                ++pos_;
            }
        }
        if (pos_ < text_.size() && (text_[pos_] == '.' || text_[pos_] == 'e' || text_[pos_] == 'E')) {
            fail("fractions and exponents are not supported");
        }
        std::int64_t value = 0;
        const char* first = text_.data() + start;
        const char* last = text_.data() + pos_;
        const auto [ptr, ec] = std::from_chars(first, last, value);
        if (ec != std::errc{} || ptr != last) {
            fail_at("integer out of 64-bit range", start);
        }
        return Json::integer(value);
    }

    std::uint32_t parse_hex4() {
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i) {
            if (pos_ >= text_.size()) {
                fail("unexpected end of input in \\u escape");
            }
            const char c = text_[pos_];
            std::uint32_t d = 0;
            if (c >= '0' && c <= '9') {
                d = static_cast<std::uint32_t>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                d = static_cast<std::uint32_t>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                d = static_cast<std::uint32_t>(c - 'A' + 10);
            } else {
                fail("invalid hex digit in \\u escape");
            }
            v = v * 16 + d;
            ++pos_;
        }
        return v;
    }

    void parse_escape(std::string& out) {
        ++pos_;  // backslash
        if (pos_ >= text_.size()) {
            fail("unterminated escape");
        }
        const char e = text_[pos_++];
        switch (e) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                std::uint32_t cp = parse_hex4();
                if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    fail_at("lone low surrogate", pos_ - 6);
                }
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    if (pos_ + 1 < text_.size() && text_[pos_] == '\\' && text_[pos_ + 1] == 'u') {
                        pos_ += 2;
                        const std::uint32_t low = parse_hex4();
                        if (low < 0xDC00 || low > 0xDFFF) {
                            fail_at("invalid low surrogate", pos_ - 6);
                        }
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    } else {
                        fail_at("lone high surrogate", pos_ - 6);
                    }
                }
                append_utf8(out, cp);
                break;
            }
            default:
                fail_at("invalid escape sequence", pos_ - 1);
        }
    }

    std::string parse_string() {
        ++pos_;  // opening quote
        std::string out;
        for (;;) {
            if (pos_ >= text_.size()) {
                fail("unterminated string");
            }
            const char c = text_[pos_];
            if (c == '"') {
                ++pos_;
                return out;
            }
            if (c == '\\') {
                parse_escape(out);
                continue;
            }
            const auto uc = static_cast<unsigned char>(c);
            if (uc < 0x20) {
                fail("unescaped control character in string");
            }
            if (uc < 0x80) {
                out += c;
                ++pos_;
                continue;
            }
            const std::size_t len = utf8_sequence_length(text_, pos_);
            if (len == 0) {
                fail("invalid UTF-8 in string");
            }
            out.append(text_.substr(pos_, len));
            pos_ += len;
        }
    }

    Json parse_array() {
        enter();
        ++pos_;  // '['
        Json::Array items;
        skip_ws();
        if (peek() == ']') {
            ++pos_;
            leave();
            return Json::array(std::move(items));
        }
        for (;;) {
            skip_ws();
            items.push_back(parse_value());
            skip_ws();
            const char c = peek();
            ++pos_;
            if (c == ']') {
                break;
            }
            if (c != ',') {
                fail_at("expected ',' or ']'", pos_ - 1);
            }
        }
        leave();
        return Json::array(std::move(items));
    }

    Json parse_object() {
        enter();
        ++pos_;  // '{'
        Json::Object members;
        std::set<std::string, std::less<>> seen;
        skip_ws();
        if (peek() == '}') {
            ++pos_;
            leave();
            return Json::object(std::move(members));
        }
        for (;;) {
            skip_ws();
            if (peek() != '"') {
                fail("expected string key");
            }
            const std::size_t key_at = pos_;
            std::string key = parse_string();
            if (!seen.insert(key).second) {
                fail_at("duplicate key", key_at);
            }
            skip_ws();
            if (peek() != ':') {
                fail("expected ':'");
            }
            ++pos_;
            skip_ws();
            Json value = parse_value();
            members.emplace_back(std::move(key), std::move(value));
            skip_ws();
            const char c = peek();
            ++pos_;
            if (c == '}') {
                break;
            }
            if (c != ',') {
                fail_at("expected ',' or '}'", pos_ - 1);
            }
        }
        leave();
        return Json::object(std::move(members));
    }

    std::string_view text_;
    std::size_t pos_ = 0;
    int depth_ = 0;
};

}  // namespace

Json Json::null() {
    return Json(Storage{std::in_place_type<std::nullptr_t>, nullptr});
}

Json Json::boolean(bool value) {
    return Json(Storage{std::in_place_type<bool>, value});
}

Json Json::integer(std::int64_t value) {
    return Json(Storage{std::in_place_type<std::int64_t>, value});
}

Json Json::string(std::string value) {
    return Json(Storage{std::in_place_type<std::string>, std::move(value)});
}

Json Json::array(Array items) {
    return Json(Storage{std::in_place_type<Array>, std::move(items)});
}

Json Json::object(Object members) {
    std::set<std::string_view> keys;
    for (const auto& member : members) {
        if (!keys.insert(member.first).second) {
            throw JsonError("duplicate key: " + member.first);
        }
    }
    return Json(Storage{std::in_place_type<Object>, std::move(members)});
}

Json::Kind Json::kind() const noexcept {
    return static_cast<Kind>(value_.index());
}

bool Json::as_bool() const {
    if (const auto* v = std::get_if<bool>(&value_)) {
        return *v;
    }
    throw JsonError("expected boolean");
}

std::int64_t Json::as_integer() const {
    if (const auto* v = std::get_if<std::int64_t>(&value_)) {
        return *v;
    }
    throw JsonError("expected integer");
}

const std::string& Json::as_string() const {
    if (const auto* v = std::get_if<std::string>(&value_)) {
        return *v;
    }
    throw JsonError("expected string");
}

const Json::Array& Json::as_array() const {
    if (const auto* v = std::get_if<Array>(&value_)) {
        return *v;
    }
    throw JsonError("expected array");
}

const Json::Object& Json::as_object() const {
    if (const auto* v = std::get_if<Object>(&value_)) {
        return *v;
    }
    throw JsonError("expected object");
}

const Json* Json::find(std::string_view key) const {
    const auto* members = std::get_if<Object>(&value_);
    if (members == nullptr) {
        throw JsonError("expected object");
    }
    for (const auto& member : *members) {
        if (member.first == key) {
            return &member.second;
        }
    }
    return nullptr;
}

std::string Json::dump(int indent) const {
    std::string out;
    write(out, indent, 0);
    return out;
}

void Json::write(std::string& out, int indent, int depth) const {
    switch (kind()) {
        case Kind::null:
            out += "null";
            return;
        case Kind::boolean:
            out += as_bool() ? "true" : "false";
            return;
        case Kind::integer:
            out += std::to_string(as_integer());
            return;
        case Kind::string:
            dump_string(out, as_string());
            return;
        case Kind::array: {
            const Array& items = as_array();
            if (items.empty()) {
                out += "[]";
                return;
            }
            out += '[';
            for (std::size_t i = 0; i < items.size(); ++i) {
                if (i > 0) {
                    out += ',';
                }
                newline_indent(out, indent, depth + 1);
                items[i].write(out, indent, depth + 1);
            }
            newline_indent(out, indent, depth);
            out += ']';
            return;
        }
        case Kind::object: {
            const Object& members = as_object();
            if (members.empty()) {
                out += "{}";
                return;
            }
            std::vector<const Object::value_type*> sorted;
            sorted.reserve(members.size());
            for (const auto& member : members) {
                sorted.push_back(&member);
            }
            std::sort(sorted.begin(), sorted.end(),
                      [](const auto* a, const auto* b) { return a->first < b->first; });
            out += '{';
            for (std::size_t i = 0; i < sorted.size(); ++i) {
                if (i > 0) {
                    out += ',';
                }
                newline_indent(out, indent, depth + 1);
                dump_string(out, sorted[i]->first);
                out += indent >= 0 ? ": " : ":";
                sorted[i]->second.write(out, indent, depth + 1);
            }
            newline_indent(out, indent, depth);
            out += '}';
            return;
        }
    }
}

Json Json::parse(std::string_view text) {
    Parser parser(text);
    return parser.parse_document();
}

}  // namespace cppref::files
