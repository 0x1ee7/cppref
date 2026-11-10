#include "hooks.hpp"

#include <functional>
#include <string_view>
#include <utility>

namespace cppref::hooks {

namespace {

// The innermost installed sink. Single-threaded by contract.
ScopedLogHook::Sink& current_sink() {
    static ScopedLogHook::Sink sink;
    return sink;
}

}  // namespace

void emit_log(std::string_view message) {
    const auto& sink = current_sink();
    if (sink) sink(message);
}

ScopedLogHook::ScopedLogHook(Sink sink) : previous_(std::move(current_sink())) {
    current_sink() = std::move(sink);
}

ScopedLogHook::~ScopedLogHook() {
    current_sink() = std::move(previous_);
}

}  // namespace cppref::hooks
