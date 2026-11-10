#include "async.hpp"

#include <cstddef>
#include <stdexcept>
#include <utility>

namespace cppref::logging {

AsyncSink::AsyncSink(std::shared_ptr<Sink> target, std::size_t capacity)
    : target_(std::move(target)), capacity_(capacity) {
    if (!target_) {
        throw std::invalid_argument("AsyncSink: target must not be null");
    }
    if (capacity_ == 0) {
        throw std::invalid_argument("AsyncSink: capacity must be >= 1");
    }
    worker_ = std::thread([this] { run(); });
}

AsyncSink::~AsyncSink() {
    {
        std::scoped_lock lock(mutex_);
        stopping_ = true;
    }
    not_empty_.notify_all();
    // run() returns only once the queue is empty, so this drains every accepted record.
    if (worker_.joinable()) worker_.join();
    try {
        target_->flush();
    } catch (...) {
        // Destructors must not throw; a failing final flush is ignored.
    }
}

void AsyncSink::write(const Record& record) {
    {
        std::scoped_lock lock(mutex_);
        if (queue_.size() >= capacity_) {
            dropped_.fetch_add(1);
            return;
        }
        queue_.push_back(record);
        ++pending_;
    }
    not_empty_.notify_one();
}

void AsyncSink::flush() {
    {
        std::unique_lock lock(mutex_);
        drained_.wait(lock, [this] { return pending_ == 0; });
    }
    target_->flush();
}

std::size_t AsyncSink::capacity() const noexcept {
    return capacity_;
}

std::uint64_t AsyncSink::dropped() const noexcept {
    return dropped_.load();
}

std::uint64_t AsyncSink::failed() const noexcept {
    return failed_.load();
}

void AsyncSink::run() {
    for (;;) {
        Record record;
        {
            std::unique_lock lock(mutex_);
            not_empty_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (queue_.empty()) return;  // stopping and nothing left to deliver
            record = std::move(queue_.front());
            queue_.pop_front();
        }

        // Deliver outside the lock so producers are never blocked by the target.
        try {
            target_->write(record);
        } catch (...) {
            failed_.fetch_add(1);
        }

        {
            std::scoped_lock lock(mutex_);
            --pending_;
            if (pending_ == 0) drained_.notify_all();
        }
    }
}

}  // namespace cppref::logging
