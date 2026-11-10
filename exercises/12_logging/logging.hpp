// Core of cppref::logging: levels, records, sinks, MDC, loggers and registry.
//
// Design rules the whole library relies on:
//   * A logger's effective level is its own level, or the nearest ancestor's
//     explicit level. Level checks must be cheap and safe to call from any
//     thread while another thread changes levels (use atomics).
//   * Formatting happens only after the level check passes. Disabled log
//     calls must not evaluate or format their arguments.
//   * Records are self-contained values: they copy everything they need
//     (including the MDC snapshot), so sinks may keep them past the call.
//   * Sinks are shared between loggers and called from many threads at once,
//     so every Sink implementation must be thread-safe.
#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <format>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace cppref::logging {

// Ordered: trace < debug < info < warn < error < off.
// `off` is only meaningful as a logger level; no record is ever emitted at it.
enum class Level : std::uint8_t { trace, debug, info, warn, error, off };

struct ParseLevelError {
    std::string input;
};

// Case-insensitive: "trace", "debug", "info", "warn" or "warning", "error", "off".
std::expected<Level, ParseLevelError> parse_level(std::string_view text);

// Upper-case name: "TRACE" ... "ERROR", "OFF".
std::string_view to_string(Level level) noexcept;

// ISO 8601 in UTC with millisecond precision: "2023-11-14T22:13:20.123Z".
std::string format_utc_iso8601(std::chrono::system_clock::time_point tp);

struct Field {
    std::string key;
    std::string value;

    friend bool operator==(const Field&, const Field&) = default;
};

// One log event, fully materialised: no references into the caller's data.
struct Record {
    std::chrono::system_clock::time_point time;
    Level level = Level::info;
    std::string logger;         // full dotted name; "" for the root logger
    std::string message;        // already rendered by std::format
    std::source_location location;
    std::thread::id thread;
    std::vector<Field> fields;  // MDC snapshot taken on the logging thread
};

// Destination for records. Implementations must be thread-safe.
class Sink {
public:
    Sink() = default;
    Sink(const Sink&) = delete;
    Sink& operator=(const Sink&) = delete;
    virtual ~Sink() = default;

    virtual void write(const Record& record) = 0;
    virtual void flush() {}
};

// ---------------------------------------------------------------------------
// Mapped Diagnostic Context: per-thread key/value fields attached to records.
// ---------------------------------------------------------------------------

// RAII scope: while alive, records logged on this thread carry key=value.
// Scopes nest LIFO. An inner scope with the same key shadows the outer one;
// the outer value comes back when the inner scope ends. Destruction restores
// state even while an exception is unwinding. Not movable or copyable: it is
// tied to the thread and stack position where it was created.
class MdcScope {
public:
    MdcScope(std::string_view key, std::string_view value);
    ~MdcScope();

    MdcScope(const MdcScope&) = delete;
    MdcScope& operator=(const MdcScope&) = delete;
    MdcScope(MdcScope&&) = delete;
    MdcScope& operator=(MdcScope&&) = delete;

    // engineer: no data members are required in this class if the thread-local
    // stack is kept at namespace scope in logging.cpp.
};

// Current fields of this thread: one entry per key (innermost value wins),
// ordered by first insertion (outermost scope first).
std::vector<Field> mdc_snapshot();

// ---------------------------------------------------------------------------
// Loggers and registry
// ---------------------------------------------------------------------------

class Registry;

class Logger {
public:
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // Full dotted name, "" for the root.
    std::string_view name() const noexcept;

    // Nearest logger above this one in the dotted hierarchy; nullptr for root.
    Logger* parent() const noexcept;

    // Explicit level of this logger; nullopt when it inherits.
    std::optional<Level> level() const noexcept;
    void set_level(Level level) noexcept;
    void clear_level() noexcept;  // back to inheriting

    // Level after inheritance is resolved; the root always has an explicit level.
    Level effective_level() const noexcept;

    // True when a record at `level` would be emitted by this logger.
    bool enabled(Level level) const noexcept;

    // When true (the default), records continue to ancestors' sinks.
    bool propagate() const noexcept;
    void set_propagate(bool propagate) noexcept;

    void add_sink(std::shared_ptr<Sink> sink);
    void clear_sinks() noexcept;

    // Formats and dispatches `fmt` with `args` if `level` is enabled.
    // Prefer the CPPREF_* macros below, which capture the call-site location.
    // Disabled calls must return before any formatting happens.
    template <class... Args>
    void log([[maybe_unused]] Level level, [[maybe_unused]] std::source_location location,
             [[maybe_unused]] std::format_string<Args...> fmt,
             [[maybe_unused]] Args&&... args) {
        throw std::logic_error("not implemented: Logger::log");
    }

private:
    friend class Registry;
    Logger(std::string name, Logger* parent);

    // Builds the Record (time, thread, MDC snapshot) and dispatches it to this
    // logger's sinks, then to ancestors while propagate() holds.
    void emit(Level level, std::source_location location, std::string message);

    // engineer: members (name, parent, atomic level with an "inherit" marker,
    // atomic propagate flag, sink list guarded by a mutex).
};

// Owns every logger. Loggers are never destroyed before the registry, so
// references returned by get() and root() stay valid for its lifetime.
// Each Registry is independent: tests create their own and nothing is global.
class Registry {
public:
    // Creates the root logger with an explicit Level::info.
    Registry();
    ~Registry();

    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;

    Logger& root();

    // Returns the logger for a dotted name ("app.db"), creating it and any
    // missing ancestors ("app") on demand. The same name always yields the same
    // object. Empty names and empty segments ("", "a..b", ".a", "a.") throw
    // std::invalid_argument. Prefix matching is on dotted segments only:
    // "application" is not a child of "app".
    Logger& get(std::string_view name);

    // engineer: members (root logger, map name -> unique_ptr<Logger> with
    // std::less<> for string_view lookup, and a mutex guarding the map).
};

// ---------------------------------------------------------------------------
// Convenience macros: capture the call-site location for every record.
// `fmt` must be a string literal so that std::format_string checks it at
// compile time.
// ---------------------------------------------------------------------------
#define CPPREF_LOG(logger, lvl, ...) \
    (logger).log((lvl), ::std::source_location::current(), __VA_ARGS__)
#define CPPREF_TRACE(logger, ...) \
    CPPREF_LOG(logger, ::cppref::logging::Level::trace, __VA_ARGS__)
#define CPPREF_DEBUG(logger, ...) \
    CPPREF_LOG(logger, ::cppref::logging::Level::debug, __VA_ARGS__)
#define CPPREF_INFO(logger, ...) \
    CPPREF_LOG(logger, ::cppref::logging::Level::info, __VA_ARGS__)
#define CPPREF_WARN(logger, ...) \
    CPPREF_LOG(logger, ::cppref::logging::Level::warn, __VA_ARGS__)
#define CPPREF_ERROR(logger, ...) \
    CPPREF_LOG(logger, ::cppref::logging::Level::error, __VA_ARGS__)

}  // namespace cppref::logging
