#include "transaction.hpp"

#include <stdexcept>
#include <utility>

namespace cppref::raii_and_memory {

void Transaction::put(std::string key, std::string value) {
    if (!active_) throw std::logic_error("transaction is not active");
    if (auto it = target_.find(key); it != target_.end()) {
        journal_.push_back(Undo{key, it->second});  // may throw: nothing changed yet
        it->second = std::move(value);              // string move-assign does not allocate
        return;
    }
    journal_.push_back(Undo{key, std::nullopt});
    try {
        target_.emplace(std::move(key), std::move(value));
    } catch (...) {
        journal_.pop_back();
        throw;
    }
}

bool Transaction::erase(std::string_view key) {
    if (!active_) throw std::logic_error("transaction is not active");
    auto it = target_.find(key);
    if (it == target_.end()) return false;
    journal_.push_back(Undo{it->first, it->second});  // may throw: nothing changed yet
    target_.erase(it);
    return true;
}

std::optional<std::string> Transaction::get(std::string_view key) const {
    auto it = target_.find(key);
    if (it == target_.end()) return std::nullopt;
    return it->second;
}

void Transaction::rollback_to(std::size_t mark) noexcept {
    // Undo in reverse order: the target always matches the state right after the entry being undone.
    while (journal_.size() > mark) {
        Undo& u = journal_.back();
        if (u.previous) {
            if (auto it = target_.find(u.key); it != target_.end()) {
                it->second = std::move(*u.previous);  // existing key: assignment, no allocation
            } else {
                target_.emplace(std::move(u.key), std::move(*u.previous));  // re-insert an erased key
            }
        } else {
            target_.erase(u.key);
        }
        journal_.pop_back();
    }
}

}  // namespace cppref::raii_and_memory
