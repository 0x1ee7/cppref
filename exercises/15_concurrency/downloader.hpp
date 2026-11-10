#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <semaphore>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cppref::concurrency {

// Blocking byte fetch. May throw; the exception is propagated to the caller.
using Fetcher = std::function<std::string(std::string_view url)>;

// Downloads with a hard cap on simultaneous fetches, enforced by a counting semaphore.
// Safe to call download() concurrently from many threads.
class BoundedDownloader {
public:
    // Throws std::invalid_argument unless 1 <= max_concurrent <= std::counting_semaphore<>::max().
    BoundedDownloader(Fetcher fetch, std::size_t max_concurrent);

    BoundedDownloader(const BoundedDownloader&) = delete;
    BoundedDownloader& operator=(const BoundedDownloader&) = delete;

    // Acquires a slot, fetches, releases the slot (also when the fetch throws).
    std::string download(std::string_view url);

    // Fetches all URLs; results keep input order. Uses min(urls.size(), 16) worker threads.
    // If any fetch throws, all workers still finish and the exception of the lowest-index
    // failing URL is rethrown.
    std::vector<std::string> download_all(std::span<const std::string> urls);

    // Fetches currently holding a slot.
    std::size_t in_flight() const;

    // Highest value in_flight() has reached since construction.
    std::size_t peak_concurrency() const;

private:
    Fetcher fetch_;
    std::counting_semaphore<> slots_;
    // Private members for the engineer to add: in_flight and peak counters (atomics).
    std::atomic<std::size_t> in_flight_{0};
    std::atomic<std::size_t> peak_{0};
};

}  // namespace cppref::concurrency
