#include "buffer.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <span>
#include <utility>

namespace cppref::raii_and_memory {

ByteBuffer::ByteBuffer(std::size_t n) : data_(std::make_unique<std::byte[]>(n)), size_(n), cap_(n) {}

ByteBuffer::ByteBuffer(std::span<const std::byte> src)
    : data_(std::make_unique_for_overwrite<std::byte[]>(src.size())), size_(src.size()), cap_(src.size()) {
    std::copy(src.begin(), src.end(), data_.get());
}

ByteBuffer::ByteBuffer(const ByteBuffer& other) : ByteBuffer(other.bytes()) {}

ByteBuffer::ByteBuffer(ByteBuffer&& other) noexcept
    : data_(std::move(other.data_)),
      size_(std::exchange(other.size_, std::size_t{0})),
      cap_(std::exchange(other.cap_, std::size_t{0})) {}

ByteBuffer& ByteBuffer::operator=(const ByteBuffer& other) {
    // Copy-and-move: the old contents are only released once the new copy exists.
    if (this != &other) *this = ByteBuffer(other);
    return *this;
}

ByteBuffer& ByteBuffer::operator=(ByteBuffer&& other) noexcept {
    if (this != &other) {
        data_ = std::move(other.data_);
        size_ = std::exchange(other.size_, std::size_t{0});
        cap_ = std::exchange(other.cap_, std::size_t{0});
    }
    return *this;
}

void ByteBuffer::append(std::span<const std::byte> more) {
    if (more.empty()) return;
    const std::size_t need = size_ + more.size();
    if (need > cap_) {
        const std::size_t new_cap = std::max(need, cap_ * 2);
        // Allocation happens before any member changes, so a throw leaves *this untouched.
        auto fresh = std::make_unique_for_overwrite<std::byte[]>(new_cap);
        std::copy_n(data_.get(), size_, fresh.get());
        data_ = std::move(fresh);
        cap_ = new_cap;
    }
    std::copy(more.begin(), more.end(), data_.get() + size_);
    size_ = need;
}

ScratchPtr make_scratch(std::size_t n, std::size_t* freed) {
    return ScratchPtr(new int[n], ScratchDeleter(freed));
}

}  // namespace cppref::raii_and_memory
