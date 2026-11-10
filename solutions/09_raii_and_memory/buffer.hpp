#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <utility>

namespace cppref::raii_and_memory {

// Owns a heap byte buffer with value semantics.
// Invariants: size() <= capacity(); a moved-from buffer is empty (size 0, capacity 0).
class ByteBuffer {
public:
    ByteBuffer() noexcept = default;
    explicit ByteBuffer(std::size_t n);
    explicit ByteBuffer(std::span<const std::byte> src);

    ByteBuffer(const ByteBuffer& other);
    ByteBuffer(ByteBuffer&& other) noexcept;
    ByteBuffer& operator=(const ByteBuffer& other);
    ByteBuffer& operator=(ByteBuffer&& other) noexcept;
    ~ByteBuffer() = default;

    std::size_t size() const noexcept { return size_; }
    std::size_t capacity() const noexcept { return cap_; }
    const std::byte* data() const noexcept { return data_.get(); }
    std::byte* data() noexcept { return data_.get(); }
    std::span<const std::byte> bytes() const noexcept { return {data_.get(), size_}; }

    void append(std::span<const std::byte> more);

private:
    std::unique_ptr<std::byte[]> data_;
    std::size_t size_ = 0;
    std::size_t cap_ = 0;
};

// Deleter that frees an int[] and counts the free into *freed. freed must outlive the pointer.
class ScratchDeleter {
public:
    explicit ScratchDeleter(std::size_t* freed) noexcept : freed_(freed) {}
    void operator()(int* p) const noexcept {
        delete[] p;
        ++*freed_;
    }

private:
    std::size_t* freed_;
};

using ScratchPtr = std::unique_ptr<int[], ScratchDeleter>;

ScratchPtr make_scratch(std::size_t n, std::size_t* freed);

}  // namespace cppref::raii_and_memory
