// Part 3: small-buffer containers that do not allocate under their inline capacity.
#pragma once

#include <cstddef>
#include <stdexcept>
#include <string_view>

namespace cppref::performance {

// Vector with N in-object slots (N > 0).
//  - Up to N elements live inside the object: no heap allocation.
//  - push_back/emplace_back beyond capacity moves the elements to one heap block (a single
//    allocation) and then grows it geometrically (capacity doubles).
//  - Moved-from objects are empty. Moving a spilled vector steals the heap block (no allocation,
//    data() pointer unchanged). Moving an inline vector moves the elements.
//  - Growth relocates with std::move_if_noexcept. push_back/emplace_back give the strong
//    guarantee: if constructing the element or relocating throws, the vector is unchanged.
//    Copy/move assignment give the basic guarantee.
//  - emplace_back may receive an argument that refers to an element of the vector.
//  - clear() keeps the current storage; pop_back() and clear() destroy the removed elements.
// noexcept: default, move constructor and move assignment are noexcept when T's move is.
template <class T, std::size_t N>
class InlineVector {
    static_assert(N > 0, "InlineVector needs at least one inline slot");

public:
    using value_type = T;
    using size_type = std::size_t;
    using iterator = T*;
    using const_iterator = const T*;

    InlineVector() { throw std::logic_error("not implemented: InlineVector()"); }
    InlineVector(const InlineVector& /*other*/) {
        throw std::logic_error("not implemented: InlineVector(const InlineVector&)");
    }
    InlineVector(InlineVector&& /*other*/) {
        throw std::logic_error("not implemented: InlineVector(InlineVector&&)");
    }
    InlineVector& operator=(const InlineVector& /*other*/) {
        throw std::logic_error("not implemented: InlineVector::operator=(const&)");
    }
    InlineVector& operator=(InlineVector&& /*other*/) {
        throw std::logic_error("not implemented: InlineVector::operator=(&&)");
    }
    ~InlineVector() {}  // engineer: destroy the elements, release the heap block if any

    void push_back(const T& /*value*/) { throw std::logic_error("not implemented: push_back"); }
    void push_back(T&& /*value*/) { throw std::logic_error("not implemented: push_back"); }
    template <class... Args>
    T& emplace_back(Args&&... /*args*/) {
        throw std::logic_error("not implemented: emplace_back");
    }
    void pop_back() { throw std::logic_error("not implemented: pop_back"); }  // precondition: !empty()
    void clear() { throw std::logic_error("not implemented: clear"); }

    size_type size() const { throw std::logic_error("not implemented: size"); }
    size_type capacity() const { throw std::logic_error("not implemented: capacity"); }
    bool empty() const { throw std::logic_error("not implemented: empty"); }
    bool is_inline() const { throw std::logic_error("not implemented: is_inline"); }

    T* data() { throw std::logic_error("not implemented: data"); }
    const T* data() const { throw std::logic_error("not implemented: data"); }
    T& operator[](size_type /*i*/) { throw std::logic_error("not implemented: operator[]"); }
    const T& operator[](size_type /*i*/) const {
        throw std::logic_error("not implemented: operator[]");
    }
    iterator begin() { throw std::logic_error("not implemented: begin"); }
    iterator end() { throw std::logic_error("not implemented: end"); }
    const_iterator begin() const { throw std::logic_error("not implemented: begin"); }
    const_iterator end() const { throw std::logic_error("not implemented: end"); }

private:
    // engineer: inline storage (aligned raw buffer of N objects), heap block pointer (null while
    // inline), size and capacity.
};

// String with N inline characters (N > 0), same storage policy as InlineVector.
//  - assign/append within N characters: no allocation. Beyond N: one allocation, then the heap
//    block grows geometrically.
//  - Copy is deep. Moving a spilled string steals the block; moved-from strings are empty.
//  - view() aliases the object's storage and is invalidated by any mutation.
template <std::size_t N>
class SmallString {
    static_assert(N > 0, "SmallString needs at least one inline character");

public:
    SmallString() { throw std::logic_error("not implemented: SmallString()"); }
    explicit SmallString(std::string_view /*text*/) {
        throw std::logic_error("not implemented: SmallString(string_view)");
    }
    SmallString(const SmallString& /*other*/) {
        throw std::logic_error("not implemented: SmallString(const SmallString&)");
    }
    SmallString(SmallString&& /*other*/) {
        throw std::logic_error("not implemented: SmallString(SmallString&&)");
    }
    SmallString& operator=(const SmallString& /*other*/) {
        throw std::logic_error("not implemented: SmallString::operator=(const&)");
    }
    SmallString& operator=(SmallString&& /*other*/) {
        throw std::logic_error("not implemented: SmallString::operator=(&&)");
    }
    ~SmallString() {}  // engineer: release the heap block if any

    void assign(std::string_view /*text*/) {
        throw std::logic_error("not implemented: assign");
    }
    void append(std::string_view /*text*/) {
        throw std::logic_error("not implemented: append");
    }
    void push_back(char /*c*/) { throw std::logic_error("not implemented: push_back"); }
    void clear() { throw std::logic_error("not implemented: clear"); }

    std::size_t size() const { throw std::logic_error("not implemented: size"); }
    std::size_t capacity() const { throw std::logic_error("not implemented: capacity"); }
    bool empty() const { throw std::logic_error("not implemented: empty"); }
    bool is_inline() const { throw std::logic_error("not implemented: is_inline"); }

    const char* data() const { throw std::logic_error("not implemented: data"); }
    std::string_view view() const { throw std::logic_error("not implemented: view"); }
    char operator[](std::size_t /*i*/) const {
        throw std::logic_error("not implemented: operator[]");
    }

private:
    // engineer: inline character buffer of N bytes, heap block pointer (null while inline), size
    // and capacity.
};

}  // namespace cppref::performance
