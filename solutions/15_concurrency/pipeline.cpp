#include "pipeline.hpp"

#include "queue.hpp"

#include <atomic>
#include <cstddef>
#include <exception>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace cppref::concurrency {
namespace {

struct Item {
    std::size_t index;
    std::string value;
};

// Keeps the first exception raised by any thread of the pipeline.
class FirstError {
public:
    void record(std::exception_ptr e) {
        std::lock_guard lock(mutex_);
        if (!first_) first_ = std::move(e);
    }

    std::exception_ptr take() {
        std::lock_guard lock(mutex_);
        return first_;
    }

private:
    std::mutex mutex_;
    std::exception_ptr first_;
};

}  // namespace

std::vector<std::string> run_pipeline(std::span<const std::string> inputs,
                                      std::span<const Stage> stages,
                                      std::size_t workers_per_stage,
                                      std::size_t queue_capacity) {
    if (workers_per_stage == 0) throw std::invalid_argument("run_pipeline: workers_per_stage must be >= 1");
    if (queue_capacity == 0) throw std::invalid_argument("run_pipeline: queue_capacity must be >= 1");
    for (const Stage& stage : stages) {
        if (!stage) throw std::invalid_argument("run_pipeline: empty stage");
    }
    if (stages.empty()) return std::vector<std::string>(inputs.begin(), inputs.end());
    if (inputs.empty()) return {};

    // One queue before each stage plus one after the last; queues[s] feeds stage s.
    std::stop_source abort;
    const std::stop_token token = abort.get_token();
    std::vector<std::unique_ptr<BoundedQueue<Item>>> queues;
    queues.reserve(stages.size() + 1);
    for (std::size_t i = 0; i <= stages.size(); ++i) {
        queues.push_back(std::make_unique<BoundedQueue<Item>>(queue_capacity));
    }

    // The last worker out of a stage closes that stage's output queue, which ends the next stage.
    std::vector<std::atomic<std::size_t>> live_workers(stages.size());
    for (auto& live : live_workers) live.store(workers_per_stage, std::memory_order_relaxed);

    FirstError errors;
    std::vector<std::string> results(inputs.size());
    auto fail = [&](std::exception_ptr e) {
        errors.record(std::move(e));
        abort.request_stop();  // wakes every blocked push/pop so all threads can exit
    };

    {
        std::vector<std::jthread> threads;  // all joined when this block ends

        threads.emplace_back([&] {
            try {
                for (std::size_t i = 0; i < inputs.size(); ++i) {
                    if (!queues.front()->push(Item{i, inputs[i]}, token)) break;
                }
            } catch (...) {
                fail(std::current_exception());
            }
            queues.front()->close();
        });

        for (std::size_t s = 0; s < stages.size(); ++s) {
            for (std::size_t w = 0; w < workers_per_stage; ++w) {
                threads.emplace_back([&, s] {
                    try {
                        while (auto item = queues[s]->pop(token)) {
                            item->value = stages[s](std::move(item->value));
                            if (!queues[s + 1]->push(std::move(*item), token)) break;
                        }
                    } catch (...) {
                        fail(std::current_exception());
                    }
                    if (live_workers[s].fetch_sub(1, std::memory_order_acq_rel) == 1) {
                        queues[s + 1]->close();
                    }
                });
            }
        }

        // Collect in the calling thread; the queue closes after the last stage drains.
        while (auto item = queues.back()->pop(token)) {
            results[item->index] = std::move(item->value);
        }
    }

    if (auto e = errors.take()) std::rethrow_exception(e);
    return results;
}

}  // namespace cppref::concurrency
