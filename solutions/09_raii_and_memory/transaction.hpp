#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cppref::raii_and_memory {

using KvMap = std::map<std::string, std::string, std::less<>>;

class Transaction;

// A mark in a Transaction's undo journal. Destroying an armed savepoint rolls back to its mark.
// Must not outlive its Transaction.
class Savepoint {
public:
    Savepoint(Savepoint&& other) noexcept
        : owner_(std::exchange(other.owner_, nullptr)), mark_(other.mark_) {}

    Savepoint& operator=(Savepoint&& other) noexcept {
        if (this != &other) {
            rollback();
            owner_ = std::exchange(other.owner_, nullptr);
            mark_ = other.mark_;
        }
        return *this;
    }

    Savepoint(const Savepoint&) = delete;
    Savepoint& operator=(const Savepoint&) = delete;

    ~Savepoint() { rollback(); }

    // Keeps the changes; enclosing rollbacks still undo them.
    void release() noexcept { owner_ = nullptr; }

    // Undoes changes since the mark and disarms. Idempotent.
    void rollback() noexcept;

private:
    friend class Transaction;
    Savepoint(Transaction* owner, std::size_t mark) noexcept : owner_(owner), mark_(mark) {}

    Transaction* owner_ = nullptr;
    std::size_t mark_ = 0;
};

// Undo-log transaction over a KvMap. Writes hit the target immediately; each mutation records
// the previous state so rollback can restore it.
class Transaction {
public:
    explicit Transaction(KvMap& target) noexcept : target_(target) {}

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    Transaction(Transaction&&) = delete;
    Transaction& operator=(Transaction&&) = delete;

    // Rolls back unless already committed. Never throws.
    ~Transaction() { rollback(); }

    // Strong guarantee. Throws std::logic_error if the transaction is no longer active.
    void put(std::string key, std::string value);

    // Returns false (and records nothing) when key is absent. Throws std::logic_error if inactive.
    bool erase(std::string_view key);

    std::optional<std::string> get(std::string_view key) const;

    // The savepoint is valid until this transaction commits or rolls back past it.
    Savepoint savepoint() noexcept { return Savepoint(this, journal_.size()); }

    // Keeps all changes and clears the journal; savepoints become no-ops. Never throws.
    void commit() noexcept {
        journal_.clear();
        active_ = false;
    }

    // Undoes everything since the start. Idempotent. Never throws.
    void rollback() noexcept {
        rollback_to(0);
        active_ = false;
    }

    bool active() const noexcept { return active_; }
    std::size_t journal_size() const noexcept { return journal_.size(); }

private:
    friend class Savepoint;

    struct Undo {
        std::string key;
        std::optional<std::string> previous;  // nullopt: the key did not exist before
    };

    void rollback_to(std::size_t mark) noexcept;

    KvMap& target_;
    std::vector<Undo> journal_;
    bool active_ = true;
};

inline void Savepoint::rollback() noexcept {
    if (owner_ != nullptr) {
        owner_->rollback_to(mark_);
        owner_ = nullptr;
    }
}

}  // namespace cppref::raii_and_memory
