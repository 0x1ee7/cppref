// Part 2 - polymorphic ownership: a value wrapper around unique_ptr<T> whose copies
// are deep copies made through T::clone().
#pragma once

#include <concepts>
#include <memory>
#include <stdexcept>

namespace cppref::polymorphism {

/// Owning value wrapper for a polymorphic T.
///
/// Requirements on T: `std::unique_ptr<T> clone() const`.
///  - copy: deep copy through clone(), so the dynamic type is preserved;
///  - move: transfers the pointer and leaves the source empty;
///  - an empty Owned (moved-from or built from nullptr) is falsy.
///
/// Engineer: add the std::unique_ptr<T> data member.
template <class T>
class Owned {
public:
    /// Takes ownership of `ptr`. U must be T or derive from T.
    template <class U>
        requires std::derived_from<U, T>
    explicit Owned(std::unique_ptr<U> /*ptr*/) {
        throw std::logic_error("not implemented: Owned(unique_ptr)");
    }

    /// Deep copy through clone(). Copying an empty Owned yields an empty Owned.
    Owned(const Owned& /*other*/) { throw std::logic_error("not implemented: Owned(const Owned&)"); }

    /// Leaves `other` empty.
    Owned(Owned&& /*other*/) { throw std::logic_error("not implemented: Owned(Owned&&)"); }

    /// Deep copy-assignment. Self-assignment must be safe.
    Owned& operator=(const Owned& /*other*/) {
        throw std::logic_error("not implemented: Owned::operator=(const Owned&)");
    }

    /// Move-assignment. Leaves `other` empty.
    Owned& operator=(Owned&& /*other*/) { throw std::logic_error("not implemented: Owned::operator=(Owned&&)"); }

    ~Owned() = default;

    /// Non-owning pointer to the target (nullptr when empty).
    [[nodiscard]] T* get() const { throw std::logic_error("not implemented: Owned::get"); }

    /// Precondition: not empty.
    [[nodiscard]] T& operator*() const { throw std::logic_error("not implemented: Owned::operator*"); }

    /// Precondition: not empty.
    [[nodiscard]] T* operator->() const { throw std::logic_error("not implemented: Owned::operator->"); }

    /// True when the wrapper holds a target.
    explicit operator bool() const { throw std::logic_error("not implemented: Owned::operator bool"); }
};

}  // namespace cppref::polymorphism
