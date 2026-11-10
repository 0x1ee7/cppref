// 02 · Parameter passing: engineer-owned API.
//
// Implement every declaration here (templates in this header, the rest in
// parameter_passing.cpp). The stubs throw std::logic_error("not implemented: ...").
// Tests measure copies/moves of an instrumented type, so the signatures below
// are a contract: do not change them.
#pragma once

#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace cppref::parameter_passing {

namespace detail {
[[noreturn]] inline void not_implemented(const char* name) {
    throw std::logic_error(std::string("not implemented: ") + name);
}
}  // namespace detail

// ---------------------------------------------------------------------------
// Part 1 · Sink by value, const&, &&
// ---------------------------------------------------------------------------

/// Holds one T. Cost contract, measured by the tests with an instrumented type:
///  - ctor and reset(T) are sinks: lvalue argument = 1 copy + 1 move;
///    named rvalue = 2 moves; prvalue = 1 move (the parameter is elided).
///  - assign(const T&): 1 copy-assign, no moves.
///  - assign(T&&): 1 move-assign, no copies.
///  - get(): returns a reference, no copies or moves.
///  - take() &&: moves the value out (1 move); the slot keeps a moved-from T.
template <class T>
class Slot {
public:
    explicit Slot([[maybe_unused]] T value) { detail::not_implemented("Slot::Slot"); }

    void reset([[maybe_unused]] T value) { detail::not_implemented("Slot::reset"); }

    void assign([[maybe_unused]] const T& value) { detail::not_implemented("Slot::assign(const&)"); }

    void assign([[maybe_unused]] T&& value) { detail::not_implemented("Slot::assign(&&)"); }

    const T& get() const { detail::not_implemented("Slot::get"); }

    T take() && { detail::not_implemented("Slot::take"); }

private:
    // Engineer: data member holding the T (e.g. `T value_;`).
};

// ---------------------------------------------------------------------------
// Part 2 · Perfect forwarding and in-place construction
// ---------------------------------------------------------------------------

/// make_unique analogue: constructs T from the forwarded arguments.
/// Each argument reaches T's constructor with its original value category.
template <class T, class... Args>
    requires std::constructible_from<T, Args...>
std::unique_ptr<T> make_owned(Args&&...) {
    detail::not_implemented("make_owned");
}

/// Name -> T registry. Values are constructed in place from forwarded arguments.
template <class T>
class Registry {
public:
    /// Constructs the value under `key` from `args`.
    /// If `key` already exists: throws std::invalid_argument and the arguments
    /// are NOT consumed (try_emplace semantics: no node is built first).
    template <class... Args>
    T& emplace(std::string_view, Args&&...) {
        detail::not_implemented("Registry::emplace");
    }

    /// Stored value, or nullptr when absent.
    const T* find(std::string_view) const { detail::not_implemented("Registry::find"); }

    std::size_t size() const { detail::not_implemented("Registry::size"); }

private:
    // Engineer: std::map<std::string, T, std::less<>> entries_;
};

// ---------------------------------------------------------------------------
// Part 3 · Defaults, options struct (named-argument idiom), non-virtual interface
// ---------------------------------------------------------------------------

enum class Method { get, post, put, patch, del, head, options };

/// Case-insensitive. Unknown text: std::invalid_argument.
Method parse_method(std::string_view text);

/// Canonical upper-case name, e.g. Method::del -> "DELETE".
std::string_view method_name(Method method);

struct Header {
    std::string name;
    std::string value;
};

/// Named-argument idiom: call sites use designated initializers, so every
/// option has a default and omitted fields keep it.
struct RequestOptions {
    std::vector<Header> headers;
    std::chrono::milliseconds timeout{5000};
    int retries = 0;             ///< must be >= 0, else std::invalid_argument
    bool follow_redirects = true;
};

struct Request {
    Method method{};
    std::string url;
    std::vector<Header> headers;  ///< names lowercased, order preserved
    std::chrono::milliseconds timeout{};
    int retries = 0;
    bool follow_redirects = true;
};

/// url must start with http:// or https:// (case-insensitive), else std::invalid_argument.
/// `options` is a sink: its headers are moved into the Request.
Request build_request(Method method, std::string_view url, RequestOptions options = {});

struct Response {
    int status = 0;
    int attempts = 0;
};

/// Base class with a non-virtual public entry point. The default argument
/// lives on the non-virtual `send`, so it is the same for every dynamic type
/// (default arguments bind statically; a virtual default would not).
class Transport {
public:
    Transport() = default;
    Transport(const Transport&) = delete;
    Transport& operator=(const Transport&) = delete;
    virtual ~Transport() = default;

    /// attempts must be >= 1, else std::invalid_argument (do_send is not called).
    Response send(const Request& request, int attempts = 3) const;

private:
    virtual Response do_send(const Request& request, int attempts) const = 0;
};

// ---------------------------------------------------------------------------
// Part 4 · optional<optional<T>>: absent, clear, or set (PATCH semantics)
// ---------------------------------------------------------------------------

struct User {
    std::string email;
    std::optional<std::string> nickname;

    bool operator==(const User&) const = default;
};

/// Outer nullopt = field absent (left untouched).
/// Outer engaged, inner nullopt = clear. Outer engaged, inner value = set.
using PatchField = std::optional<std::optional<std::string>>;

struct UserPatch {
    PatchField email;
    PatchField nickname;
};

/// Takes the user by value and returns the patched copy; the caller's object is
/// never modified. Email: trimmed, lowercased, must match
/// [^@\s]+@[^@\s]+\.[^@\s]+ ; clearing it is invalid. Nickname: trimmed,
/// 1..32 characters, or cleared with an inner nullopt.
/// Any violation: std::invalid_argument.
User apply_patch(User user, const UserPatch& patch);

// ---------------------------------------------------------------------------
// Part 5 · string_view / span parameters and overload sets
// ---------------------------------------------------------------------------

/// Splits `line` on `separator`. Empty fields are kept ("a,,b" -> 3 fields;
/// "" -> one empty field). The returned views ALIAS `line`: they are valid only
/// while the caller's buffer lives, and no text is copied.
std::vector<std::string_view> split_fields(std::string_view line, char separator);

/// Joins with `separator`; the span is read, never retained.
std::string join_fields(std::span<const std::string_view> fields, char separator);

/// Overload set for log rendering. Strings are quoted: format_value("abc") is
/// "\"abc\"", never "true" (a const char* must not decay to bool).
/// bool -> "true"/"false"; int64 -> decimal; double -> std::format("{}", v).
/// NOTE: a plain `int` argument is ambiguous between these overloads; tests cast.
std::string format_value(bool value);
std::string format_value(std::int64_t value);
std::string format_value(double value);
std::string format_value(std::string_view value);
std::string format_value(const char* value);  ///< never null

// ---------------------------------------------------------------------------
// Part 6 · Variadic parameters and keyword-style options for a query builder
// ---------------------------------------------------------------------------

/// Bound parameter: SQL NULL, bool, 64-bit integer, or text.
using Value = std::variant<std::nullptr_t, bool, std::int64_t, std::string>;

struct Sql {
    std::string text;
    std::vector<Value> params;  ///< in placeholder order

    bool operator==(const Sql&) const = default;
};

struct SelectOptions {
    /// Column -> value, rendered as `col = ?` joined by AND in key order.
    /// A nullptr value renders `col IS NULL` with no parameter.
    std::map<std::string, Value, std::less<>> where;
    /// "col" = ascending, "-col" = DESC. Terms are rendered in order.
    std::vector<std::string> order_by;
    /// Renders `LIMIT ?`. Negative: std::invalid_argument.
    std::optional<std::int64_t> limit;
};

/// Builds `SELECT <columns or *> FROM <table> [WHERE ...] [ORDER BY ...] [LIMIT ?]`.
/// Every identifier (table, columns, where keys, order terms) must match
/// [A-Za-z_][A-Za-z0-9_]* with at most one dotted part ("u.id");
/// anything else: std::invalid_argument. Values are only ever placeholders.
template <class... Columns>
    requires(std::convertible_to<const Columns&, std::string_view> && ...)
Sql select(std::string_view table, const SelectOptions& options, [[maybe_unused]] const Columns&... columns) {
    (void)table;
    (void)options;
    (void)sizeof...(columns);
    detail::not_implemented("select");
}

// ---------------------------------------------------------------------------
// Part 7 · Event bus: forwarding arguments to listeners
// ---------------------------------------------------------------------------

struct SubscribeOptions {
    bool once = false;  ///< removed before it is called
};

using SubscriptionId = std::uint64_t;

/// Typed bus. publish takes its arguments by value (one sink per publish);
/// listeners borrow them as const&, so a listener adds no copy of its own.
template <class... Args>
class EventBus {
public:
    using Listener = std::function<void(const Args&...)>;

    /// Listeners run in subscription order. An empty listener: std::invalid_argument.
    SubscriptionId subscribe(std::string_view, Listener, SubscribeOptions = {}) {
        detail::not_implemented("EventBus::subscribe");
    }

    /// Removes the subscription with this id from any event. False if absent.
    bool unsubscribe(SubscriptionId) { detail::not_implemented("EventBus::unsubscribe"); }

    /// Calls every listener of `event` with the arguments and returns how many ran.
    /// Iterates over a snapshot: listeners scheduled in this publish still run even
    /// if unsubscribed meanwhile; subscribers added during it do not run.
    /// A once-listener is removed before it runs and never runs twice.
    /// A throwing listener propagates out of publish; later listeners do not run.
    std::size_t publish(std::string_view, Args...) { detail::not_implemented("EventBus::publish"); }

private:
    // Engineer: per-event list of {id, listener, once}, plus the next id.
};

}  // namespace cppref::parameter_passing
