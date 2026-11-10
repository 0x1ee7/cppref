#pragma once

#include <functional>
#include <string_view>

// Part 6a — RAII guards around process-wide state.
namespace cppref::hooks {

// Delivers a message to the innermost installed sink. Does nothing if no sink is installed.
// Single-threaded use only in this exercise.
void emit_log(std::string_view message);

// RAII guard: installs `sink` for its lifetime and restores the previously installed sink
// on destruction. Guards nest like a stack. Not copyable and not movable.
class ScopedLogHook {
public:
    using Sink = std::function<void(std::string_view)>;

    explicit ScopedLogHook(Sink sink);
    ~ScopedLogHook();

    ScopedLogHook(const ScopedLogHook&) = delete;
    ScopedLogHook& operator=(const ScopedLogHook&) = delete;
    ScopedLogHook(ScopedLogHook&&) = delete;
    ScopedLogHook& operator=(ScopedLogHook&&) = delete;

private:
    Sink previous_;
};

}  // namespace cppref::hooks
