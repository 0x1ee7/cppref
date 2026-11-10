#pragma once

#include <cstddef>
#include <memory>
#include <span>

namespace cppref::raii_and_memory {

// Owns a heap byte buffer with value semantics.
// Invariants: size() <= capacity(); a moved-from buffer is empty (size 0, capacity 0).
// Engineer-owned: storage (for example std::unique_ptr<std::byte[]>), size and capacity.
class ByteBuffer {
public:
    ByteBuffer() noexcept;
    // Zero-filled buffer of n bytes.
    explicit ByteBuffer(std::size_t n);
    // Copies src.
    explicit ByteBuffer(std::span<const std::byte> src);

    // Deep copy.
    ByteBuffer(const ByteBuffer& other);
    // Takes other's storage; other becomes empty.
    ByteBuffer(ByteBuffer&& other) noexcept;
    // Strong guarantee; self-assignment is safe.
    ByteBuffer& operator=(const ByteBuffer& other);
    // Releases this storage and takes other's; self-move is a no-op.
    ByteBuffer& operator=(ByteBuffer&& other) noexcept;
    ~ByteBuffer();

    std::size_t size() const noexcept;
    std::size_t capacity() const noexcept;
    const std::byte* data() const noexcept;
    std::byte* data() noexcept;
    std::span<const std::byte> bytes() const noexcept;

    // Appends; grows capacity geometrically. Strong guarantee.
    void append(std::span<const std::byte> more);
};

// Stateful deleter for ScratchPtr. Frees an int[] allocated with new[] and adds 1 to *freed.
// Engineer-owned: store the counter pointer.
class ScratchDeleter {
public:
    explicit ScratchDeleter(std::size_t* freed) noexcept { (void)freed; }
    void operator()(int* p) const noexcept { (void)p; }
};

using ScratchPtr = std::unique_ptr<int[], ScratchDeleter>;

// Allocates n default-initialized ints owned by a ScratchPtr whose deleter counts into *freed.
ScratchPtr make_scratch(std::size_t n, std::size_t* freed);

}  // namespace cppref::raii_and_memory
