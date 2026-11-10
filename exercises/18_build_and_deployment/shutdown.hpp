// Part 4 · Graceful shutdown: first SIGINT/SIGTERM requests a stop; handlers are restored on exit.
#pragma once

#include <initializer_list>
#include <optional>
#include <stop_token>

namespace cppref::deployment {

/// RAII signal handling. At most one SignalGuard may be alive at a time.
///
/// Construction installs a handler for each signal and remembers the previous one.
/// The first delivery of any guarded signal requests a stop on token(); later deliveries
/// are ignored. Destruction restores the previous handlers.
///
/// Handlers must be async-signal-safe. Your design must not call request_stop() or
/// allocate inside the handler. Use a lock-free flag and let a watcher thread do the rest.
class SignalGuard {
public:
    /// Throws std::logic_error if another SignalGuard is alive,
    /// std::invalid_argument if a signal cannot be handled.
    explicit SignalGuard(std::initializer_list<int> signals);
    ~SignalGuard();

    SignalGuard(const SignalGuard&) = delete;
    SignalGuard& operator=(const SignalGuard&) = delete;

    /// Becomes stop-requested after the first guarded signal.
    [[nodiscard]] std::stop_token token() const;

    /// The first signal delivered, or nullopt if none yet.
    [[nodiscard]] std::optional<int> received_signal() const;

private:
    // Engineer-owned members: saved previous handlers, stop source, received-signal
    // slot, and the watcher thread that bridges the handler to the stop source.
};

}  // namespace cppref::deployment
