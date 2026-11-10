// Part 2 - polymorphic ownership: a value wrapper around unique_ptr<T> whose copies
// are deep copies made through T::clone().
#pragma once

#include <concepts>
#include <memory>
#include <utility>

namespace cppref::polymorphism {

/// Owning value wrapper for a polymorphic T.
///
/// Requirements on T: `std::unique_ptr<T> clone() const`.
///  - copy: deep copy through clone(), so the dynamic type is preserved;
///  - move: transfers the pointer and leaves the source empty;
///  - an empty Owned (moved-from or built from nullptr) is falsy.
template <class T>
class Owned {
public:
    /// Takes ownership of `ptr`. U must be T or derive from T.
    template <class U>
        requires std::derived_from<U, T>
    explicit Owned(std::unique_ptr<U> ptr) : ptr_(std::move(ptr)) {}

    /// Deep copy through clone(). Copying an empty Owned yields an empty Owned.
    Owned(const Owned& other) : ptr_(other.ptr_ ? other.ptr_->clone() : std::unique_ptr<T>{}) {}

    /// Leaves `other` empty.
    Owned(Owned&& other) : ptr_(std::move(other.ptr_)) {}

    /// Deep copy-assignment. The clone is made before the old target is released,
    /// so a throwing clone() leaves *this unchanged.
    Owned& operator=(const Owned& other) {
        if (this != &other) {
            ptr_ = other.ptr_ ? other.ptr_->clone() : std::unique_ptr<T>{};
        }
        return *this;
    }

    /// Move-assignment. Leaves `other` empty.
    Owned& operator=(Owned&& other) {
        if (this != &other) {
            ptr_ = std::move(other.ptr_);
        }
        return *this;
    }

    ~Owned() = default;

    /// Non-owning pointer to the target (nullptr when empty).
    [[nodiscard]] T* get() const { return ptr_.get(); }

    /// Precondition: not empty.
    [[nodiscard]] T& operator*() const { return *ptr_; }

    /// Precondition: not empty.
    [[nodiscard]] T* operator->() const { return ptr_.get(); }

    /// True when the wrapper holds a target.
    explicit operator bool() const { return ptr_ != nullptr; }

private:
    std::unique_ptr<T> ptr_;
};

}  // namespace cppref::polymorphism
