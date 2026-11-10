#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

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
    explicit ThreadPool(std::size_t threads);

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Equivalent to shutdown(ShutdownMode::drain).
    ~ThreadPool();

    // Queues fn for execution and returns a future for its result. Throws std::runtime_error
    // if the pool has been shut down.
    template <typename F>
    auto submit(F&& fn) -> std::future<std::invoke_result_t<std::decay_t<F>&>> {
        using R = std::invoke_result_t<std::decay_t<F>&>;
        // The packaged_task lives in a shared_ptr so the queued std::function stays copyable;
        // dropping an unrun job releases the last reference and breaks the promise.
        auto task = std::make_shared<std::packaged_task<R()>>(std::forward<F>(fn));
        std::future<R> future = task->get_future();
        enqueue([task] { (*task)(); });
        return future;
    }

    // Stops accepting work and joins all workers. Idempotent; a second call with a different mode
    // is a no-op. Must not be called from a worker thread of this pool.
    void shutdown(ShutdownMode mode = ShutdownMode::drain);

    // Number of queued tasks that have not started yet.
    std::size_t pending() const;

private:
    void enqueue(std::function<void()> job);
    void worker_loop(std::stop_token stop);

    mutable std::mutex mutex_;
    std::condition_variable_any cv_;
    std::deque<std::function<void()>> queue_;
    bool accepting_ = true;
    std::vector<std::jthread> workers_;  // declared last: joined before the other members die
};

}  // namespace cppref::concurrency
