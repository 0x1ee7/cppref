// Part 3: small-buffer containers that do not allocate under their inline capacity.
#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>

namespace cppref::performance {

// Vector with N in-object slots (N > 0). Storage is either the inline buffer (heap_ == nullptr)
// or one heap block; capacity_ is N while inline.
template <class T, std::size_t N>
class InlineVector {
    static_assert(N > 0, "InlineVector needs at least one inline slot");

public:
    using value_type = T;
    using size_type = std::size_t;
    using iterator = T*;
    using const_iterator = const T*;

    InlineVector() = default;

    InlineVector(const InlineVector& other) {
        if (other.size_ > N) {
            heap_ = allocate(other.size_);
            capacity_ = other.size_;
        }
        try {
            std::uninitialized_copy_n(other.data(), other.size_, data());
        } catch (...) {
            if (heap_ != nullptr) deallocate(heap_, capacity_);
            throw;
        }
        size_ = other.size_;
    }

    InlineVector(InlineVector&& other) noexcept(std::is_nothrow_move_constructible_v<T>) {
        take(other);
    }

    InlineVector& operator=(const InlineVector& other) {
        if (this != &other) *this = InlineVector(other);
        return *this;
    }

    InlineVector& operator=(InlineVector&& other) noexcept(std::is_nothrow_move_constructible_v<T>) {
        if (this != &other) {
            release();
            take(other);
        }
        return *this;
    }

    ~InlineVector() { release(); }

    void push_back(const T& value) { emplace_back(value); }
    void push_back(T&& value) { emplace_back(std::move(value)); }

    template <class... Args>
    T& emplace_back(Args&&... args) {
        if (size_ < capacity_) {
            T* slot = std::construct_at(data() + size_, std::forward<Args>(args)...);
            ++size_;
            return *slot;
        }

        // Spill / grow. The new element is built first: args may refer to an element that is
        // about to be relocated, and it must still be valid while it is read.
        const size_type new_cap = capacity_ * 2;
        T* block = allocate(new_cap);
        try {
            std::construct_at(block + size_, std::forward<Args>(args)...);
        } catch (...) {
            deallocate(block, new_cap);
            throw;
        }
        try {
            relocate_prefix(block);
        } catch (...) {
            std::destroy_at(block + size_);
            deallocate(block, new_cap);
            throw;
        }

        std::destroy_n(data(), size_);
        if (heap_ != nullptr) deallocate(heap_, capacity_);
        heap_ = block;
        capacity_ = new_cap;
        return block[size_++];
    }

    void pop_back() { std::destroy_at(data() + --size_); }

    void clear() {
        std::destroy_n(data(), size_);
        size_ = 0;
    }

    size_type size() const { return size_; }
    size_type capacity() const { return capacity_; }
    bool empty() const { return size_ == 0; }
    bool is_inline() const { return heap_ == nullptr; }

    T* data() { return heap_ != nullptr ? heap_ : inline_ptr(); }
    const T* data() const { return heap_ != nullptr ? heap_ : inline_ptr(); }
    T& operator[](size_type i) { return data()[i]; }
    const T& operator[](size_type i) const { return data()[i]; }
    iterator begin() { return data(); }
    iterator end() { return data() + size_; }
    const_iterator begin() const { return data(); }
    const_iterator end() const { return data() + size_; }

private:
    static T* allocate(size_type n) { return std::allocator<T>{}.allocate(n); }
    static void deallocate(T* p, size_type n) noexcept { std::allocator<T>{}.deallocate(p, n); }

    T* inline_ptr() noexcept { return std::launder(reinterpret_cast<T*>(buf_)); }
    const T* inline_ptr() const noexcept { return std::launder(reinterpret_cast<const T*>(buf_)); }

    // Moves (or copies, when the move may throw) the existing elements into `block`. On failure
    // the constructed prefix is destroyed and the old elements are untouched.
    void relocate_prefix(T* block) {
        size_type i = 0;
        try {
            for (; i < size_; ++i) std::construct_at(block + i, std::move_if_noexcept(data()[i]));
        } catch (...) {
            std::destroy_n(block, i);
            throw;
        }
    }

    // Precondition: *this is empty and inline. Steals a heap block, or moves inline elements.
    void take(InlineVector& other) noexcept(std::is_nothrow_move_constructible_v<T>) {
        if (other.heap_ != nullptr) {
            heap_ = std::exchange(other.heap_, nullptr);
            capacity_ = std::exchange(other.capacity_, N);
            size_ = std::exchange(other.size_, 0);
            return;
        }
        if constexpr (std::is_nothrow_move_constructible_v<T>) {
            for (; size_ < other.size_; ++size_) {
                std::construct_at(inline_ptr() + size_, std::move(other.inline_ptr()[size_]));
            }
        } else {
            try {
                for (; size_ < other.size_; ++size_) {
                    std::construct_at(inline_ptr() + size_, std::move(other.inline_ptr()[size_]));
                }
            } catch (...) {
                std::destroy_n(inline_ptr(), size_);
                size_ = 0;
                throw;
            }
        }
        other.clear();
    }

    void release() noexcept {
        std::destroy_n(data(), size_);
        if (heap_ != nullptr) deallocate(heap_, capacity_);
        heap_ = nullptr;
        capacity_ = N;
        size_ = 0;
    }

    alignas(T) std::byte buf_[sizeof(T) * N];
    T* heap_ = nullptr;
    size_type size_ = 0;
    size_type capacity_ = N;
};

// String with N inline characters (N > 0), same storage policy as InlineVector.
template <std::size_t N>
class SmallString {
    static_assert(N > 0, "SmallString needs at least one inline character");

public:
    SmallString() = default;
    explicit SmallString(std::string_view text) { assign(text); }
    SmallString(const SmallString& other) { assign(other.view()); }
    SmallString(SmallString&& other) noexcept { take(other); }

    SmallString& operator=(const SmallString& other) {
        if (this != &other) *this = SmallString(other);
        return *this;
    }
    SmallString& operator=(SmallString&& other) noexcept {
        if (this != &other) {
            release();
            take(other);
        }
        return *this;
    }

    ~SmallString() { release(); }

    void assign(std::string_view text) {
        if (text.size() > capacity_) {
            // Copy from `text` before the old block is freed: it may alias our own storage.
            char* block = allocate(text.size());
            std::copy_n(text.data(), text.size(), block);
            if (heap_ != nullptr) deallocate(heap_, capacity_);
            heap_ = block;
            capacity_ = text.size();
        } else {
            std::copy_n(text.data(), text.size(), writable());
        }
        size_ = text.size();
    }

    void append(std::string_view text) {
        const std::size_t needed = size_ + text.size();
        if (needed > capacity_) {
            const std::size_t new_cap = std::max(needed, capacity_ * 2);
            char* block = allocate(new_cap);
            std::copy_n(writable(), size_, block);
            std::copy_n(text.data(), text.size(), block + size_);
            if (heap_ != nullptr) deallocate(heap_, capacity_);
            heap_ = block;
            capacity_ = new_cap;
        } else {
            std::copy_n(text.data(), text.size(), writable() + size_);
        }
        size_ = needed;
    }

    void push_back(char c) { append(std::string_view(&c, 1)); }
    void clear() noexcept { size_ = 0; }

    std::size_t size() const noexcept { return size_; }
    std::size_t capacity() const noexcept { return capacity_; }
    bool empty() const noexcept { return size_ == 0; }
    bool is_inline() const noexcept { return heap_ == nullptr; }

    const char* data() const noexcept { return heap_ != nullptr ? heap_ : inline_; }
    std::string_view view() const noexcept { return {data(), size_}; }
    char operator[](std::size_t i) const noexcept { return data()[i]; }

private:
    static char* allocate(std::size_t n) { return std::allocator<char>{}.allocate(n); }
    static void deallocate(char* p, std::size_t n) noexcept { std::allocator<char>{}.deallocate(p, n); }

    char* writable() noexcept { return heap_ != nullptr ? heap_ : inline_; }

    // Precondition: *this is empty and inline.
    void take(SmallString& other) noexcept {
        if (other.heap_ != nullptr) {
            heap_ = std::exchange(other.heap_, nullptr);
            capacity_ = std::exchange(other.capacity_, N);
        } else {
            std::copy_n(other.inline_, other.size_, inline_);
        }
        size_ = std::exchange(other.size_, 0);
    }

    void release() noexcept {
        if (heap_ != nullptr) deallocate(heap_, capacity_);
        heap_ = nullptr;
        capacity_ = N;
        size_ = 0;
    }

    char inline_[N];
    char* heap_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = N;
};

}  // namespace cppref::performance
