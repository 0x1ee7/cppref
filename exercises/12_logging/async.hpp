// Non-blocking sink: a bounded queue drained by a single worker thread.
#pragma once

#include "logging.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace cppref::logging {

// Decouples producers from a slow target sink. Producers call write(), which
// copies the record into a bounded queue and returns immediately; if the queue
// is full the record is dropped and counted instead of waiting.
//
// Lifecycle: the worker thread starts in the constructor. The destructor
// delivers every accepted record, flushes the target, and joins the worker.
// The target is shared, so it stays alive while the worker runs.
//
// Exceptions thrown by the target's write() are caught by the worker, counted
// in failed(), and never reach the logging call site. flush() still completes.
class AsyncSink final : public Sink {
public:
    // Preconditions (std::invalid_argument): target != nullptr, capacity >= 1.
    AsyncSink(std::shared_ptr<Sink> target, std::size_t capacity);
    ~AsyncSink() override;

    AsyncSink(const AsyncSink&) = delete;
    AsyncSink& operator=(const AsyncSink&) = delete;

    // Never blocks. Returns after queueing, or after counting a drop.
    void write(const Record& record) override;

    // Blocks until every record accepted so far has been handed to the target
    // (including any still being written), then flushes the target.
    void flush() override;

    std::size_t capacity() const noexcept;
    // Records rejected because the queue was full.
    std::uint64_t dropped() const noexcept;
    // Target write() calls that threw.
    std::uint64_t failed() const noexcept;

    // engineer: members (target, capacity, std::deque<Record> queue, mutex,
    // condition variables, pending count, atomic counters, std::thread worker).
};

}  // namespace cppref::logging
