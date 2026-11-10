#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>

namespace cppref::error_handling {

/// Tallies shared by every Probe copied or moved from the same origin.
struct ProbeCounts {
    std::size_t copies = 0;
    std::size_t moves = 0;
};

/// Copyable value used to observe what std::vector does on reallocation.
/// NothrowMove is the knob. Its move constructor must be declared
/// `noexcept(NothrowMove)`. The declaration below deliberately omits that
/// specifier; adding it is part of Part 4.
template <bool NothrowMove>
class Probe {
public:
    Probe([[maybe_unused]] int value, [[maybe_unused]] std::shared_ptr<ProbeCounts> counts) {
        throw std::logic_error("not implemented: Probe::Probe");
    }

    /// Counts one copy.
    Probe([[maybe_unused]] const Probe& other) {
        throw std::logic_error("not implemented: Probe copy constructor");
    }

    /// Counts one move. Make this `noexcept(NothrowMove)`.
    Probe([[maybe_unused]] Probe&& other) {
        throw std::logic_error("not implemented: Probe move constructor");
    }

    Probe& operator=(const Probe&) = delete;
    Probe& operator=(Probe&&) = delete;
    ~Probe() = default;

    [[nodiscard]] int value() const { throw std::logic_error("not implemented: Probe::value"); }

private:
    int value_ = 0;
    std::shared_ptr<ProbeCounts> counts_;
};

}  // namespace cppref::error_handling
