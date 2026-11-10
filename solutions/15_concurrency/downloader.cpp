#include "downloader.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <exception>
#include <semaphore>
#include <stdexcept>
#include <thread>
#include <utility>

namespace cppref::concurrency {
namespace {

std::ptrdiff_t checked_slots(std::size_t max_concurrent) {
    if (max_concurrent == 0 ||
        max_concurrent > static_cast<std::size_t>(std::counting_semaphore<>::max())) {
        throw std::invalid_argument("BoundedDownloader: max_concurrent out of range");
    }
    return static_cast<std::ptrdiff_t>(max_concurrent);
}

// Counts the call as in flight for its lifetime and releases the semaphore slot on exit.
class SlotGuard {
public:
    SlotGuard(std::counting_semaphore<>& slots, std::atomic<std::size_t>& in_flight)
        : slots_(slots), in_flight_(in_flight) {
        in_flight_.fetch_add(1, std::memory_order_acq_rel);
    }
    ~SlotGuard() {
        in_flight_.fetch_sub(1, std::memory_order_acq_rel);
        slots_.release();
    }
    SlotGuard(const SlotGuard&) = delete;
    SlotGuard& operator=(const SlotGuard&) = delete;

private:
    std::counting_semaphore<>& slots_;
    std::atomic<std::size_t>& in_flight_;
};

}  // namespace

BoundedDownloader::BoundedDownloader(Fetcher fetch, std::size_t max_concurrent)
    : fetch_(std::move(fetch)), slots_(checked_slots(max_concurrent)) {}

std::string BoundedDownloader::download(std::string_view url) {
    slots_.acquire();
    SlotGuard guard(slots_, in_flight_);

    // Read-modify-write loop: raise the peak to the current count if it is higher.
    const std::size_t now = in_flight_.load(std::memory_order_acquire);
    std::size_t peak = peak_.load(std::memory_order_relaxed);
    while (now > peak && !peak_.compare_exchange_weak(peak, now, std::memory_order_relaxed)) {
    }

    return fetch_(url);
}

std::vector<std::string> BoundedDownloader::download_all(std::span<const std::string> urls) {
    std::vector<std::string> results(urls.size());
    std::vector<std::exception_ptr> errors(urls.size());
    if (urls.empty()) return results;

    const std::size_t workers = std::min<std::size_t>(urls.size(), 16);
    std::atomic<std::size_t> next{0};
    {
        std::vector<std::jthread> threads;
        threads.reserve(workers);
        for (std::size_t w = 0; w < workers; ++w) {
            threads.emplace_back([&] {
                for (std::size_t i = next.fetch_add(1); i < urls.size(); i = next.fetch_add(1)) {
                    try {
                        results[i] = download(urls[i]);
                    } catch (...) {
                        errors[i] = std::current_exception();
                    }
                }
            });
        }
    }  // joins every worker before any error is inspected

    for (const auto& e : errors) {
        if (e) std::rethrow_exception(e);  // lowest index first
    }
    return results;
}

std::size_t BoundedDownloader::in_flight() const {
    return in_flight_.load(std::memory_order_acquire);
}

std::size_t BoundedDownloader::peak_concurrency() const {
    return peak_.load(std::memory_order_acquire);
}

}  // namespace cppref::concurrency
