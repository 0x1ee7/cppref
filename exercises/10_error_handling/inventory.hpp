#pragma once

#include <concepts>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <vector>

namespace cppref::error_handling {

/// Stock ledger over copyable entries, with explicit exception-safety levels.
/// Entry must be copyable: copy-and-swap needs copies. Tests inject throwing
/// copy constructors to observe what each operation leaves behind.
template <std::copyable Entry>
class Inventory {
public:
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }

    [[nodiscard]] std::span<const Entry> entries() const noexcept { return entries_; }

    void clear() noexcept { entries_.clear(); }

    /// Appends `batch`. Strong guarantee: if any copy throws, *this is exactly
    /// as it was before the call. `batch` may view this ledger's own entries.
    void apply([[maybe_unused]] std::span<const Entry> batch) {
        throw std::logic_error("not implemented: Inventory::apply");
    }

    /// Appends `batch` one entry at a time. Basic guarantee only: if a copy
    /// throws, the entries applied so far stay applied, and *this remains a
    /// valid ledger. `batch` must not view this ledger.
    void apply_in_place([[maybe_unused]] std::span<const Entry> batch) {
        throw std::logic_error("not implemented: Inventory::apply_in_place");
    }

private:
    std::vector<Entry> entries_;
};

}  // namespace cppref::error_handling
