#include "scheduler.hpp"

#include <chrono>
#include <exception>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace cppref::concurrency {

PeriodicScheduler::~PeriodicScheduler() {
    stop();
}

void PeriodicScheduler::add(std::string name, std::chrono::milliseconds interval,
                            std::function<void()> fn) {
    std::lock_guard lock(mutex_);
    if (started_) throw std::logic_error("PeriodicScheduler: add() after start()");
    if (interval <= std::chrono::milliseconds::zero()) {
        throw std::invalid_argument("PeriodicScheduler: interval must be > 0");
    }
    for (const Job& job : jobs_) {
        if (job.name == name) throw std::invalid_argument("PeriodicScheduler: duplicate job " + name);
    }
    jobs_.push_back(Job{std::move(name), interval, std::move(fn)});
}

void PeriodicScheduler::start() {
    std::lock_guard lock(mutex_);
    if (started_) throw std::logic_error("PeriodicScheduler: start() called twice");
    started_ = true;
    threads_.reserve(jobs_.size());
    for (const Job& job : jobs_) {
        threads_.emplace_back([this, &job](std::stop_token stop) { run_job(job, stop); });
    }
}

void PeriodicScheduler::stop() {
    {
        std::lock_guard lock(mutex_);
        if (!started_ || stopped_) return;
        stopped_ = true;
    }
    for (auto& thread : threads_) thread.request_stop();
    threads_.clear();  // each jthread joins on destruction
}

std::vector<JobError> PeriodicScheduler::errors() const {
    std::lock_guard lock(mutex_);
    return errors_;
}

void PeriodicScheduler::run_job(const Job& job, std::stop_token stop) {
    while (!stop.stop_requested()) {
        try {
            job.fn();
        } catch (const std::exception& e) {
            record(job.name, e.what());
        } catch (...) {
            record(job.name, "unknown error");
        }

        // The stop-aware wait returns as soon as stop is requested, so stop() never waits an interval.
        std::unique_lock lock(mutex_);
        cv_.wait_for(lock, stop, job.interval, [&stop] { return stop.stop_requested(); });
    }
}

void PeriodicScheduler::record(const std::string& name, std::string message) {
    std::lock_guard lock(mutex_);
    errors_.push_back(JobError{name, std::move(message)});
}

}  // namespace cppref::concurrency
