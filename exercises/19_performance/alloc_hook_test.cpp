// Replaces the global allocation functions for this executable so that every heap allocation
// increments a thread-local counter (see AllocScope in perf_support_test.hpp). Defines no tests.
#include "perf_support_test.hpp"

#include <cstdlib>
#include <new>

namespace {

void* counted_alloc(std::size_t size) {
    ++cppref::performance::testing::g_allocations;
    cppref::performance::testing::g_allocated_bytes += size;
    if (void* p = std::malloc(size == 0 ? 1 : size)) return p;
    throw std::bad_alloc{};
}

}  // namespace

void* operator new(std::size_t size) { return counted_alloc(size); }
void* operator new[](std::size_t size) { return counted_alloc(size); }

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return counted_alloc(size);
    } catch (...) {
        return nullptr;
    }
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return counted_alloc(size);
    } catch (...) {
        return nullptr;
    }
}

void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }
