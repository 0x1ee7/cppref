#include "buffer.hpp"

#include <stdexcept>

namespace cppref::raii_and_memory {

ByteBuffer::ByteBuffer() noexcept {}

ByteBuffer::ByteBuffer(std::size_t n) {
    (void)n;
    throw std::logic_error("not implemented: ByteBuffer(size_t)");
}

ByteBuffer::ByteBuffer(std::span<const std::byte> src) {
    (void)src;
    throw std::logic_error("not implemented: ByteBuffer(span)");
}

ByteBuffer::ByteBuffer(const ByteBuffer& other) {
    (void)other;
    throw std::logic_error("not implemented: ByteBuffer copy ctor");
}

ByteBuffer::ByteBuffer(ByteBuffer&& other) noexcept {
    (void)other;
}

ByteBuffer& ByteBuffer::operator=(const ByteBuffer& other) {
    (void)other;
    throw std::logic_error("not implemented: ByteBuffer copy assignment");
}

ByteBuffer& ByteBuffer::operator=(ByteBuffer&& other) noexcept {
    (void)other;
    return *this;
}

ByteBuffer::~ByteBuffer() {}

std::size_t ByteBuffer::size() const noexcept { return 0; }
std::size_t ByteBuffer::capacity() const noexcept { return 0; }
const std::byte* ByteBuffer::data() const noexcept { return nullptr; }
std::byte* ByteBuffer::data() noexcept { return nullptr; }
std::span<const std::byte> ByteBuffer::bytes() const noexcept { return {}; }

void ByteBuffer::append(std::span<const std::byte> more) {
    (void)more;
    throw std::logic_error("not implemented: ByteBuffer::append");
}

ScratchPtr make_scratch(std::size_t n, std::size_t* freed) {
    (void)n;
    (void)freed;
    throw std::logic_error("not implemented: make_scratch");
}

}  // namespace cppref::raii_and_memory
