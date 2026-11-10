// 02 · Parameter passing: reference solution, non-template definitions.
#include "parameter_passing.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cppref::parameter_passing {
namespace {

constexpr std::size_t kMaxNicknameLength = 32;
constexpr std::array<std::string_view, 7> kMethodNames{"GET", "POST", "PUT", "PATCH", "DELETE", "HEAD", "OPTIONS"};

constexpr bool is_space(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

constexpr char to_lower_ascii(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

constexpr char to_upper_ascii(char c) noexcept {
    return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
}

constexpr bool is_ident_start(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

constexpr bool is_ident_char(char c) noexcept {
    return is_ident_start(c) || (c >= '0' && c <= '9');
}

std::string_view trim(std::string_view text) noexcept {
    while (!text.empty() && is_space(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && is_space(text.back())) {
        text.remove_suffix(1);
    }
    return text;
}

std::string lower(std::string_view text) {
    std::string out(text);
    for (char& c : out) {
        c = to_lower_ascii(c);
    }
    return out;
}

// `lowercase_prefix` must already be lowercase.
bool starts_with_ci(std::string_view text, std::string_view lowercase_prefix) noexcept {
    if (text.size() < lowercase_prefix.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lowercase_prefix.size(); ++i) {
        if (to_lower_ascii(text[i]) != lowercase_prefix[i]) {
            return false;
        }
    }
    return true;
}

bool is_plain_identifier(std::string_view name) noexcept {
    if (name.empty() || !is_ident_start(name.front())) {
        return false;
    }
    return std::all_of(name.begin() + 1, name.end(), [](char c) { return is_ident_char(c); });
}

// [A-Za-z_][A-Za-z0-9_]* with at most one dotted part.
bool is_identifier(std::string_view name) noexcept {
    const std::size_t dot = name.find('.');
    if (dot == std::string_view::npos) {
        return is_plain_identifier(name);
    }
    return is_plain_identifier(name.substr(0, dot)) && is_plain_identifier(name.substr(dot + 1));
}

void require_identifier(std::string_view name, const char* what) {
    if (!is_identifier(name)) {
        throw std::invalid_argument(std::string("invalid ") + what + ": " + std::string(name));
    }
}

// Mirrors [^@\s]+@[^@\s]+\.[^@\s]+ on an already-trimmed, lowercased address.
bool is_valid_email(std::string_view email) noexcept {
    if (std::any_of(email.begin(), email.end(), [](char c) { return is_space(c); })) {
        return false;
    }
    const std::size_t at = email.find('@');
    if (at == std::string_view::npos || at == 0 || email.find('@', at + 1) != std::string_view::npos) {
        return false;
    }
    const std::string_view domain = email.substr(at + 1);
    // A dot with at least one character on each side.
    for (std::size_t i = 1; i + 1 < domain.size(); ++i) {
        if (domain[i] == '.') {
            return true;
        }
    }
    return false;
}

}  // namespace

// ---------------------------------------------------------------------------
// Part 3
// ---------------------------------------------------------------------------

Method parse_method(std::string_view text) {
    for (std::size_t i = 0; i < kMethodNames.size(); ++i) {
        const std::string_view name = kMethodNames[i];
        const bool same = name.size() == text.size() &&
                          std::equal(name.begin(), name.end(), text.begin(),
                                     [](char expected, char actual) { return expected == to_upper_ascii(actual); });
        if (same) {
            return static_cast<Method>(i);
        }
    }
    throw std::invalid_argument("unknown HTTP method: " + std::string(text));
}

std::string_view method_name(Method method) {
    const auto index = static_cast<std::size_t>(method);
    if (index >= kMethodNames.size()) {
        throw std::invalid_argument("invalid Method value");
    }
    return kMethodNames[index];
}

Request build_request(Method method, std::string_view url, RequestOptions options) {
    if (options.retries < 0) {
        throw std::invalid_argument("retries must be >= 0");
    }
    if (!starts_with_ci(url, "http://") && !starts_with_ci(url, "https://")) {
        throw std::invalid_argument("url must start with http:// or https://");
    }
    // `options` is our own sink copy: normalise in place, then move the storage.
    for (Header& header : options.headers) {
        header.name = lower(header.name);
    }
    return Request{
        .method = method,
        .url = std::string(url),
        .headers = std::move(options.headers),
        .timeout = options.timeout,
        .retries = options.retries,
        .follow_redirects = options.follow_redirects,
    };
}

Response Transport::send(const Request& request, int attempts) const {
    if (attempts < 1) {
        throw std::invalid_argument("attempts must be >= 1");
    }
    return do_send(request, attempts);
}

// ---------------------------------------------------------------------------
// Part 4
// ---------------------------------------------------------------------------

User apply_patch(User user, const UserPatch& patch) {
    if (patch.email) {
        if (!patch.email->has_value()) {
            throw std::invalid_argument("email cannot be cleared");
        }
        std::string email = lower(trim(**patch.email));
        if (!is_valid_email(email)) {
            throw std::invalid_argument("malformed email: " + email);
        }
        user.email = std::move(email);
    }
    if (patch.nickname) {
        if (!patch.nickname->has_value()) {
            user.nickname.reset();
        } else {
            const std::string_view nickname = trim(**patch.nickname);
            if (nickname.empty() || nickname.size() > kMaxNicknameLength) {
                throw std::invalid_argument("nickname must be 1..32 characters");
            }
            user.nickname = std::string(nickname);
        }
    }
    return user;
}

// ---------------------------------------------------------------------------
// Part 5
// ---------------------------------------------------------------------------

std::vector<std::string_view> split_fields(std::string_view line, char separator) {
    std::vector<std::string_view> fields;
    std::size_t start = 0;
    while (true) {
        const std::size_t pos = line.find(separator, start);
        if (pos == std::string_view::npos) {
            fields.push_back(line.substr(start));
            break;
        }
        fields.push_back(line.substr(start, pos - start));
        start = pos + 1;
    }
    return fields;
}

std::string join_fields(std::span<const std::string_view> fields, char separator) {
    std::size_t total = 0;
    for (std::string_view field : fields) {
        total += field.size() + 1;
    }
    std::string out;
    out.reserve(total);
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (i != 0) {
            out.push_back(separator);
        }
        out.append(fields[i]);
    }
    return out;
}

std::string format_value(bool value) {
    return value ? "true" : "false";
}

std::string format_value(std::int64_t value) {
    return std::to_string(value);
}

std::string format_value(double value) {
    return std::format("{}", value);
}

std::string format_value(std::string_view value) {
    std::string out;
    out.reserve(value.size() + 2);
    out.push_back('"');
    out.append(value);
    out.push_back('"');
    return out;
}

std::string format_value(const char* value) {
    return format_value(std::string_view(value));
}

// ---------------------------------------------------------------------------
// Part 6
// ---------------------------------------------------------------------------

namespace detail {

Sql select_impl(std::string_view table, const SelectOptions& options, std::span<const std::string_view> columns) {
    require_identifier(table, "table name");

    Sql sql;
    sql.text = "SELECT ";
    if (columns.empty()) {
        sql.text += '*';
    }
    for (std::size_t i = 0; i < columns.size(); ++i) {
        require_identifier(columns[i], "column");
        if (i != 0) {
            sql.text += ", ";
        }
        sql.text += columns[i];
    }

    sql.text += " FROM ";
    sql.text += table;

    if (!options.where.empty()) {
        sql.text += " WHERE ";
        bool first = true;
        for (const auto& [column, value] : options.where) {
            require_identifier(column, "column");
            if (!first) {
                sql.text += " AND ";
            }
            first = false;
            sql.text += column;
            if (std::holds_alternative<std::nullptr_t>(value)) {
                sql.text += " IS NULL";
            } else {
                sql.text += " = ?";
                sql.params.push_back(value);
            }
        }
    }

    if (!options.order_by.empty()) {
        sql.text += " ORDER BY ";
        for (std::size_t i = 0; i < options.order_by.size(); ++i) {
            std::string_view term = options.order_by[i];
            const bool descending = !term.empty() && term.front() == '-';
            if (descending) {
                term.remove_prefix(1);
            }
            require_identifier(term, "order_by column");
            if (i != 0) {
                sql.text += ", ";
            }
            sql.text += term;
            if (descending) {
                sql.text += " DESC";
            }
        }
    }

    if (options.limit) {
        if (*options.limit < 0) {
            throw std::invalid_argument("limit must be >= 0");
        }
        sql.text += " LIMIT ?";
        sql.params.emplace_back(std::in_place_type<std::int64_t>, *options.limit);
    }
    return sql;
}

}  // namespace detail

}  // namespace cppref::parameter_passing
