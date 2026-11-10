#pragma once

#include <cstddef>
#include <memory>
#include <utility>

namespace cppref::error_handling {

/// Tallies shared by every Probe copied or moved from the same origin.
struct ProbeCounts {
    std::size_t copies = 0;
    std::size_t moves = 0;
};

/// Copyable value used to observe what std::vector does on reallocation.
/// NothrowMove is the knob: the move constructor is `noexcept(NothrowMove)`,
/// so std::vector moves on growth only when that is true.
template <bool NothrowMove>
class Probe {
public:
    Probe(int value, std::shared_ptr<ProbeCounts> counts) : value_(value), counts_(std::move(counts)) {}

    /// Counts one copy.
    Probe(const Probe& other) : value_(other.value_), counts_(other.counts_) {
        if (counts_) {
            ++counts_->copies;
        }
    }

    /// Counts one move. Shared-pointer copy cannot throw, so the only throwing
    /// behaviour is the one the template parameter chooses.
    Probe(Probe&& other) noexcept(NothrowMove) : value_(other.value_), counts_(other.counts_) {
        if (counts_) {
            ++counts_->moves;
        }
    }

    Probe& operator=(const Probe&) = delete;
    Probe& operator=(Probe&&) = delete;
    ~Probe() = default;

    [[nodiscard]] int value() const { return value_; }

private:
    int value_ = 0;
    std::shared_ptr<ProbeCounts> counts_;
};

}  // namespace cppref::error_handling
