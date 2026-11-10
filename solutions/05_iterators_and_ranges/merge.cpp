#include "merge.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace cppref::iterators_and_ranges {

Generator<LogRecord> merge_sorted(std::vector<Generator<LogRecord>> streams) {
    // `streams` lives in this frame, so the element addresses behind `its` stay stable.
    std::vector<Generator<LogRecord>::iterator> its;
    its.reserve(streams.size());
    for (auto& stream : streams) {
        its.push_back(stream.begin());
    }

    // Min-heap of stream indices keyed by (head ts, stream index). std heaps are max-heaps,
    // so "later" means "should sit below".
    auto later = [&its](std::size_t a, std::size_t b) {
        const auto ta = (*its[a]).ts;
        const auto tb = (*its[b]).ts;
        if (ta != tb) {
            return ta > tb;
        }
        return a > b;
    };

    std::vector<std::size_t> heap;
    for (std::size_t i = 0; i < streams.size(); ++i) {
        if (its[i] != streams[i].end()) {
            heap.push_back(i);
        }
    }
    std::make_heap(heap.begin(), heap.end(), later);

    while (!heap.empty()) {
        std::pop_heap(heap.begin(), heap.end(), later);
        const std::size_t i = heap.back();
        heap.pop_back();

        LogRecord record = *its[i];
        ++its[i];
        if (its[i] != streams[i].end()) {
            heap.push_back(i);
            std::push_heap(heap.begin(), heap.end(), later);
        }
        co_yield std::move(record);
    }
}

}  // namespace cppref::iterators_and_ranges
