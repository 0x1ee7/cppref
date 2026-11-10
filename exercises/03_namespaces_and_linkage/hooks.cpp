#include "hooks.hpp"

#include <stdexcept>

namespace cppref::hooks {

void emit_log(std::string_view) {
    throw std::logic_error("not implemented: emit_log");
}

// The constructor throws, so the object is never fully constructed and the destructor
// never runs against the stub. Defaulted so the stub never throws from a destructor.
ScopedLogHook::ScopedLogHook(Sink) {
    throw std::logic_error("not implemented: ScopedLogHook::ScopedLogHook");
}

ScopedLogHook::~ScopedLogHook() = default;

}  // namespace cppref::hooks
