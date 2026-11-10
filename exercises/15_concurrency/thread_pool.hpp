#pragma once

#include <cstddef>
#include <future>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace cppref::concurrency {

enum class ShutdownMode {
    drain,   // run every already-queued task, then stop
    cancel,  // drop queued tasks (their futures report broken_promise) and request stop on running workers
};

// A fixed-size thread pool. Tasks are submitted with submit(); the returned future carries the
// result or the exception the task threw.
class ThreadPool {
public:
    // Throws std::invalid_argument if threads == 0.
    explicit ThreadPool(std::size_t threads) {
        (void)threads;
        throw std::logic_error("not implemented: ThreadPool::ThreadPool");
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Equivalent to shutdown(ShutdownMode::drain).
    ~ThreadPool();

    // Queues fn for execution and returns a future for its result. Throws std::runtime_error
    // if the pool has been shut down.
    template <typename F>
    auto submit(F&& fn) -> std::future<std::invoke_result_t<std::decay_t<F>&>> {
        (void)fn;
        throw std::logic_error("not implemented: ThreadPool::submit");
    }

    // Stops accepting work and joins all workers. Idempotent; a second call with a different mode
    // is a no-op. Must not be called from a worker thread of this pool.
    void shutdown(ShutdownMode mode = ShutdownMode::drain);

    // Number of queued tasks that have not started yet.
    std::size_t pending() const;

private:
    // Private members for the engineer to add: workers, task queue, mutex, condition_variable_any,
    // stopping flag. Each queued job should be wrapped so the packaged_task can be moved in.
};

}  // namespace cppref::concurrency
