// 02 · Parameter passing: reference solution.
#pragma once

#include <algorithm>
#include <array>
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

// ---------------------------------------------------------------------------
// Part 1 · Sink by value, const&, &&
// ---------------------------------------------------------------------------

/// Sink-by-value setter: one constructor/assignment per value, the caller
/// chooses the cost by how it passes the argument.
template <class T>
class Slot {
public:
    explicit Slot(T value) : value_(std::move(value)) {}

    void reset(T value) { value_ = std::move(value); }

    void assign(const T& value) { value_ = value; }

    void assign(T&& value) { value_ = std::move(value); }

    const T& get() const noexcept { return value_; }

    T take() && { return std::move(value_); }

private:
    T value_;
};

// ---------------------------------------------------------------------------
// Part 2 · Perfect forwarding and in-place construction
// ---------------------------------------------------------------------------

template <class T, class... Args>
    requires std::constructible_from<T, Args...>
std::unique_ptr<T> make_owned(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template <class T>
class Registry {
public:
    /// try_emplace: the arguments are untouched when the key already exists.
    template <class... Args>
    T& emplace(std::string_view key, Args&&... args) {
        auto [it, inserted] = entries_.try_emplace(std::string(key), std::forward<Args>(args)...);
        if (!inserted) {
            throw std::invalid_argument("duplicate key: " + std::string(key));
        }
        return it->second;
    }

    const T* find(std::string_view key) const {
        const auto it = entries_.find(key);
        return it == entries_.end() ? nullptr : &it->second;
    }

    std::size_t size() const noexcept { return entries_.size(); }

private:
    std::map<std::string, T, std::less<>> entries_;
};

// ---------------------------------------------------------------------------
// Part 3 · Defaults, options struct, non-virtual interface
// ---------------------------------------------------------------------------

enum class Method { get, post, put, patch, del, head, options };

Method parse_method(std::string_view text);

std::string_view method_name(Method method);

struct Header {
    std::string name;
    std::string value;
};

struct RequestOptions {
    std::vector<Header> headers;
    std::chrono::milliseconds timeout{5000};
    int retries = 0;
    bool follow_redirects = true;
};

struct Request {
    Method method{};
    std::string url;
    std::vector<Header> headers;
    std::chrono::milliseconds timeout{};
    int retries = 0;
    bool follow_redirects = true;
};

Request build_request(Method method, std::string_view url, RequestOptions options = {});

struct Response {
    int status = 0;
    int attempts = 0;
};

class Transport {
public:
    Transport() = default;
    Transport(const Transport&) = delete;
    Transport& operator=(const Transport&) = delete;
    virtual ~Transport() = default;

    /// Non-virtual: the default argument is fixed for every dynamic type.
    Response send(const Request& request, int attempts = 3) const;

private:
    virtual Response do_send(const Request& request, int attempts) const = 0;
};

// ---------------------------------------------------------------------------
// Part 4 · optional<optional<T>>: absent, clear, or set
// ---------------------------------------------------------------------------

struct User {
    std::string email;
    std::optional<std::string> nickname;

    bool operator==(const User&) const = default;
};

using PatchField = std::optional<std::optional<std::string>>;

struct UserPatch {
    PatchField email;
    PatchField nickname;
};

User apply_patch(User user, const UserPatch& patch);

// ---------------------------------------------------------------------------
// Part 5 · string_view / span parameters and overload sets
// ---------------------------------------------------------------------------

std::vector<std::string_view> split_fields(std::string_view line, char separator);

std::string join_fields(std::span<const std::string_view> fields, char separator);

std::string format_value(bool value);
std::string format_value(std::int64_t value);
std::string format_value(double value);
std::string format_value(std::string_view value);
std::string format_value(const char* value);

// ---------------------------------------------------------------------------
// Part 6 · Variadic parameters and keyword-style options for a query builder
// ---------------------------------------------------------------------------

using Value = std::variant<std::nullptr_t, bool, std::int64_t, std::string>;

struct Sql {
    std::string text;
    std::vector<Value> params;

    bool operator==(const Sql&) const = default;
};

struct SelectOptions {
    std::map<std::string, Value, std::less<>> where;
    std::vector<std::string> order_by;
    std::optional<std::int64_t> limit;
};

namespace detail {
/// Non-template core: validates identifiers and renders the statement.
Sql select_impl(std::string_view table, const SelectOptions& options, std::span<const std::string_view> columns);
}  // namespace detail

/// Variadic front end: only converts the column arguments to views (no copies
/// of the column text), then hands off to the non-template core.
template <class... Columns>
    requires(std::convertible_to<const Columns&, std::string_view> && ...)
Sql select(std::string_view table, const SelectOptions& options, const Columns&... columns) {
    const std::array<std::string_view, sizeof...(Columns)> cols{std::string_view(columns)...};
    return detail::select_impl(table, options, cols);
}

// ---------------------------------------------------------------------------
// Part 7 · Event bus: forwarding arguments to listeners
// ---------------------------------------------------------------------------

struct SubscribeOptions {
    bool once = false;
};

using SubscriptionId = std::uint64_t;

template <class... Args>
class EventBus {
public:
    using Listener = std::function<void(const Args&...)>;

    SubscriptionId subscribe(std::string_view event, Listener listener, SubscribeOptions options = {}) {
        if (!listener) {
            throw std::invalid_argument("listener must not be empty");
        }
        const SubscriptionId id = next_id_++;
        topics_[std::string(event)].push_back(Entry{id, std::move(listener), options.once});
        return id;
    }

    bool unsubscribe(SubscriptionId id) {
        for (auto topic = topics_.begin(); topic != topics_.end(); ++topic) {
            auto& entries = topic->second;
            const auto entry = std::find_if(entries.begin(), entries.end(),
                                            [id](const Entry& e) { return e.id == id; });
            if (entry == entries.end()) {
                continue;
            }
            entries.erase(entry);
            if (entries.empty()) {
                topics_.erase(topic);
            }
            return true;
        }
        return false;
    }

    /// Arguments are sunk once here; every listener borrows them as const&.
    std::size_t publish(std::string_view event, Args... args) {
        const auto topic = topics_.find(event);
        if (topic == topics_.end()) {
            return 0;
        }
        // Snapshot: subscriptions changed by listeners do not affect this publish.
        const std::vector<Entry> snapshot = topic->second;
        std::size_t invoked = 0;
        for (const Entry& entry : snapshot) {
            // A once-entry is removed before it runs; if it is already gone
            // (nested publish or earlier unsubscribe), it is skipped.
            if (entry.once && !unsubscribe(entry.id)) {
                continue;
            }
            entry.listener(args...);
            ++invoked;
        }
        return invoked;
    }

private:
    struct Entry {
        SubscriptionId id;
        Listener listener;
        bool once;
    };

    std::map<std::string, std::vector<Entry>, std::less<>> topics_;
    SubscriptionId next_id_ = 1;
};

}  // namespace cppref::parameter_passing
