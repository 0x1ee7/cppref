#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace cppref::raii_and_memory {

using KvMap = std::map<std::string, std::string, std::less<>>;

class Transaction;

// Marks a point inside a Transaction. Destroying it without release() rolls the transaction back
// to the mark. Must not outlive its Transaction (checked only in debug builds; see README).
class Savepoint {
public:
    Savepoint(Savepoint&& other) noexcept { (void)other; }
    Savepoint& operator=(Savepoint&& other) noexcept {
        (void)other;
        return *this;
    }
    Savepoint(const Savepoint&) = delete;
    Savepoint& operator=(const Savepoint&) = delete;

    // Rolls back to the mark unless already released, rolled back, or moved from. Never throws.
    ~Savepoint() {}

    // Keeps the changes made since the mark. Enclosing rollbacks still undo them.
    void release() noexcept {}

    // Undoes changes made since the mark now. The savepoint is then disarmed.
    void rollback() noexcept {}

private:
    friend class Transaction;
    // Engineer-owned: Transaction* owner_ and std::size_t mark_.
    Savepoint() = default;
};

// Undo-log transaction over a KvMap. Writes apply to the target immediately; an undo journal
// lets rollback restore prior state. Destroying an uncommitted transaction rolls it back.
class Transaction {
public:
    // Begins a transaction over target. target must outlive the Transaction.
    explicit Transaction(KvMap& target);

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    Transaction(Transaction&&) = delete;
    Transaction& operator=(Transaction&&) = delete;

    // Rolls back unless commit() or rollback() was already called. Never throws.
    ~Transaction() {}

    // Inserts or overwrites key. Strong guarantee: on throw (bad_alloc) nothing changes.
    void put(std::string key, std::string value) { (void)key; (void)value; }

    // Erases key. Returns false when it was absent (no journal entry is recorded then).
    bool erase(std::string_view key) { (void)key; return false; }

    // Reads through the uncommitted state.
    std::optional<std::string> get(std::string_view key) const { (void)key; return std::nullopt; }

    // Starts a savepoint at the current journal position.
    Savepoint savepoint() { return Savepoint{}; }

    // Makes the changes permanent and invalidates all savepoints. Never throws.
    void commit() noexcept {}

    // Undoes every change since the transaction began. Never throws.
    void rollback() noexcept {}

    // True until commit() or rollback() has been called.
    bool active() const noexcept { return false; }

    // Number of undo entries currently recorded.
    std::size_t journal_size() const noexcept { return 0; }

private:
    // Engineer-owned: KvMap& target_; std::vector<undo entries> journal_; bool active_.
};

}  // namespace cppref::raii_and_memory
