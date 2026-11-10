#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

namespace cppref::concurrency {

// A job failure recorded by the scheduler; the job keeps running on its next tick.
struct JobError {
    std::string name;
    std::string message;

    bool operator==(const JobError&) const = default;
};

// Runs named jobs on a fixed interval, each on its own thread, until stop() is called.
// - add() must happen before start(); job names are unique.
// - A job that throws records a JobError and is scheduled again.
// - stop() interrupts sleeping jobs immediately (it never waits out an interval) and joins
//   every thread. Idempotent; a no-op before start().
class PeriodicScheduler {
public:
    PeriodicScheduler() = default;
    PeriodicScheduler(const PeriodicScheduler&) = delete;
    PeriodicScheduler& operator=(const PeriodicScheduler&) = delete;

    // Equivalent to stop().
    ~PeriodicScheduler();

    // Throws std::invalid_argument if interval <= 0 or name is already registered,
    // and std::logic_error if the scheduler has already been started.
    void add(std::string name, std::chrono::milliseconds interval, std::function<void()> fn);

    // Launches one thread per job. Throws std::logic_error if already started (or stopped).
    void start();

    // Requests stop, wakes sleeping jobs, joins all threads.
    void stop();

    // Snapshot of every recorded job failure, in the order they happened.
    std::vector<JobError> errors() const;

private:
    struct Job {
        std::string name;
        std::chrono::milliseconds interval;
        std::function<void()> fn;
    };

    void run_job(const Job& job, std::stop_token stop);
    void record(const std::string& name, std::string message);

    mutable std::mutex mutex_;
    std::condition_variable_any cv_;
    std::vector<Job> jobs_;           // fixed once started, so job threads may hold references
    std::vector<JobError> errors_;
    std::vector<std::jthread> threads_;
    bool started_ = false;
    bool stopped_ = false;
};

}  // namespace cppref::concurrency
