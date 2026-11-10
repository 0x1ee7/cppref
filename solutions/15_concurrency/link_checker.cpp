#include "link_checker.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cppref::concurrency {

std::vector<LinkResult> check_links(std::span<const std::string> urls, const Fetch& fetch,
                                    std::stop_token stop, std::size_t max_workers) {
    if (max_workers == 0) throw std::invalid_argument("check_links: max_workers must be >= 1");

    // Each distinct URL gets one slot; every input position maps onto its slot.
    std::vector<std::string_view> distinct;
    std::vector<std::size_t> slot_of(urls.size());
    std::unordered_map<std::string_view, std::size_t> index;
    for (std::size_t i = 0; i < urls.size(); ++i) {
        auto [it, inserted] = index.try_emplace(urls[i], distinct.size());
        if (inserted) distinct.push_back(urls[i]);
        slot_of[i] = it->second;
    }

    // Slots start as "cancelled"; a worker overwrites a slot only when it actually runs it.
    std::vector<LinkResult> by_slot(distinct.size());
    for (std::size_t s = 0; s < distinct.size(); ++s) {
        by_slot[s].url = std::string(distinct[s]);
        by_slot[s].error = "cancelled";
    }

    std::atomic<std::size_t> next{0};
    auto worker = [&] {
        while (!stop.stop_requested()) {
            const std::size_t s = next.fetch_add(1, std::memory_order_relaxed);
            if (s >= by_slot.size()) return;
            LinkResult& out = by_slot[s];  // exclusive: each index is claimed once
            try {
                out.status = fetch(distinct[s], stop);
                out.error.reset();
            } catch (const std::exception& e) {
                out.error = e.what();
            } catch (...) {
                out.error = "unknown error";
            }
        }
    };

    {
        // jthreads join at the end of this block, so no worker outlives the call.
        const std::size_t n = std::min(max_workers, by_slot.size());
        std::vector<std::jthread> threads;
        threads.reserve(n);
        for (std::size_t i = 0; i < n; ++i) threads.emplace_back(worker);
    }

    std::vector<LinkResult> results;
    results.reserve(urls.size());
    for (std::size_t i = 0; i < urls.size(); ++i) {
        LinkResult r = by_slot[slot_of[i]];
        r.url = std::string(urls[i]);
        results.push_back(std::move(r));
    }
    return results;
}

}  // namespace cppref::concurrency
