#include "redact.hpp"

#include <array>
#include <cstddef>
#include <utility>

namespace cppref::logging {

namespace {

constexpr std::string_view kMask = "***";

constexpr std::array<std::string_view, 9> kSecretKeys = {
    "password",  "passwd",     "secret",        "token",          "api_key",
    "api-key",   "access_token", "client_secret", "authorization",
};

char ascii_lower(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

bool equals_ignore_case(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (ascii_lower(a[i]) != ascii_lower(b[i])) return false;
    }
    return true;
}

bool is_key_char(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '_' || c == '-';
}

bool is_quote(char c) noexcept {
    return c == '"' || c == '\'';
}

bool is_space(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// Unquoted values end at whitespace or one of these delimiters.
bool is_value_end(char c) noexcept {
    return is_space(c) || c == ',' || c == '&' || c == '}' || c == ';';
}

std::size_t skip_blanks(std::string_view text, std::size_t pos) noexcept {
    while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t')) ++pos;
    return pos;
}

// End of an unquoted value that starts at `begin`. The Authorization value
// is "rest of line" because it commonly contains spaces ("Basic abc").
std::size_t unquoted_value_end(std::string_view key, std::string_view text,
                               std::size_t begin) noexcept {
    if (equals_ignore_case(key, "authorization")) {
        std::size_t end = begin;
        while (end < text.size() && text[end] != '\r' && text[end] != '\n') ++end;
        return end;
    }
    std::size_t end = begin;
    while (end < text.size() && !is_value_end(text[end])) ++end;
    return end;
}

}  // namespace

bool is_secret_key(std::string_view key) noexcept {
    for (std::string_view secret : kSecretKeys) {
        if (equals_ignore_case(key, secret)) return true;
    }
    return false;
}

std::string redact(std::string_view text) {
    std::string out;
    out.reserve(text.size());

    std::size_t i = 0;
    while (i < text.size()) {
        if (!is_key_char(text[i])) {
            out.push_back(text[i]);
            ++i;
            continue;
        }

        // Consume a whole key token so "my_password_hint" is never split.
        std::size_t key_end = i;
        while (key_end < text.size() && is_key_char(text[key_end])) ++key_end;
        const std::string_view key = text.substr(i, key_end - i);

        bool masked = false;
        if (is_secret_key(key)) {
            // Shape: [quote] key [quote] blanks (= | :) blanks value
            std::size_t p = key_end;
            if (p < text.size() && is_quote(text[p])) ++p;  // closing quote of 'key'
            p = skip_blanks(text, p);
            if (p < text.size() && (text[p] == '=' || text[p] == ':')) {
                p = skip_blanks(text, p + 1);
                std::size_t value_begin = 0;
                std::size_t value_end = 0;
                bool have_value = false;
                if (p < text.size() && is_quote(text[p])) {
                    const std::size_t close = text.find(text[p], p + 1);
                    if (close != std::string_view::npos) {
                        value_begin = p + 1;
                        value_end = close;
                        have_value = true;
                    }
                } else {
                    value_begin = p;
                    value_end = unquoted_value_end(key, text, p);
                    have_value = true;
                }
                if (have_value && value_end > value_begin) {
                    out.append(text.substr(i, value_begin - i));
                    out.append(kMask);
                    i = value_end;
                    masked = true;
                }
            }
        }

        if (!masked) {
            out.append(key);
            i = key_end;
        }
    }
    return out;
}

RedactingSink::RedactingSink(std::shared_ptr<Sink> target) : target_(std::move(target)) {}

void RedactingSink::write(const Record& record) {
    Record scrubbed = record;
    scrubbed.message = redact(record.message);
    for (Field& field : scrubbed.fields) {
        if (is_secret_key(field.key)) field.value = std::string(kMask);
    }
    target_->write(scrubbed);
}

void RedactingSink::flush() {
    target_->flush();
}

}  // namespace cppref::logging
