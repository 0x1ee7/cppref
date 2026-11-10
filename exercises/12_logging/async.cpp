#include "async.hpp"

#include <stdexcept>

namespace cppref::logging {

AsyncSink::AsyncSink(std::shared_ptr<Sink>, std::size_t) {
    throw std::logic_error("not implemented: AsyncSink");
}

AsyncSink::~AsyncSink() = default;

void AsyncSink::write(const Record&) {
    throw std::logic_error("not implemented: AsyncSink::write");
}

void AsyncSink::flush() {
    throw std::logic_error("not implemented: AsyncSink::flush");
}

std::size_t AsyncSink::capacity() const noexcept {
    return 0;
}

std::uint64_t AsyncSink::dropped() const noexcept {
    return 0;
}

std::uint64_t AsyncSink::failed() const noexcept {
    return 0;
}

}  // namespace cppref::logging
