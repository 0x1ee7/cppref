// Shared helpers for the *_test.cpp files: per-thread allocation counter and an instrumented
// value type. The global operator new/delete replacement lives in alloc_hook_test.cpp, the only
// TU that defines it.
#pragma once

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace cppref::performance::testing {

// Maintained by the operator new replacement. Thread-local so that gtest bookkeeping on other
// threads never leaks into a measurement.
inline thread_local std::size_t g_allocations = 0;
inline thread_local std::size_t g_allocated_bytes = 0;

// Counts heap allocations made on this thread between construction and allocations().
class AllocScope {
public:
    AllocScope() noexcept : start_(g_allocations) {}
    std::size_t allocations() const noexcept { return g_allocations - start_; }

private:
    std::size_t start_;
};

// Allocations a default-constructed container makes by itself. MSVC Debug iterator checking
// (_ITERATOR_DEBUG_LEVEL=2) allocates a container proxy on construction, so budgets subtract this.
template <class C>
std::size_t container_overhead() {
    AllocScope scope;
    [[maybe_unused]] C c{};
    return scope.allocations();
}

// Process-wide counters for Probe. Call reset() at the start of every test.
struct Ledger {
    inline static int constructions = 0;
    inline static int copies = 0;
    inline static int moves = 0;
    inline static int live = 0;
    // -1: copies never fail. N >= 0: the copy after N successful copies throws std::runtime_error.
    inline static int copies_until_throw = -1;

    static void reset() noexcept {
        constructions = 0;
        copies = 0;
        moves = 0;
        live = 0;
        copies_until_throw = -1;
    }
};

// Instrumented value. NothrowMove selects whether the move constructor is noexcept, which decides
// whether std::move_if_noexcept picks the move or the copy.
template <bool NothrowMove>
class Probe {
public:
    explicit Probe(int id) : id_(id) {
        ++Ledger::constructions;
        ++Ledger::live;
    }
    Probe(const Probe& other) : id_(other.id_) {
        if (Ledger::copies_until_throw == 0) throw std::runtime_error("injected copy failure");
        if (Ledger::copies_until_throw > 0) --Ledger::copies_until_throw;
        ++Ledger::copies;
        ++Ledger::live;
    }
    Probe(Probe&& other) noexcept(NothrowMove) : id_(other.id_) {
        ++Ledger::moves;
        ++Ledger::live;
    }
    Probe& operator=(const Probe& other) {
        id_ = other.id_;
        ++Ledger::copies;
        return *this;
    }
    Probe& operator=(Probe&& other) noexcept(NothrowMove) {
        id_ = other.id_;
        ++Ledger::moves;
        return *this;
    }
    ~Probe() { --Ledger::live; }

    int id() const noexcept { return id_; }

private:
    int id_;
};

template <class V>
std::vector<int> ids_of(const V& items) {
    std::vector<int> ids;
    for (const auto& item : items) ids.push_back(item.id());
    return ids;
}

}  // namespace cppref::performance::testing
