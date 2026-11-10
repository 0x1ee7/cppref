#include "layout.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace cppref::performance {

SampleColumns to_columns(std::span<const Sample> samples) {
    SampleColumns cols;
    cols.timestamp_ns.reserve(samples.size());
    cols.value.reserve(samples.size());
    cols.sensor_id.reserve(samples.size());
    for (const Sample& s : samples) {
        cols.timestamp_ns.push_back(s.timestamp_ns);
        cols.value.push_back(s.value);
        cols.sensor_id.push_back(s.sensor_id);
    }
    return cols;
}

std::vector<SensorSummary> summarize_by_sensor(const SampleColumns& columns) {
    std::vector<SensorSummary> out;
    const std::size_t n = columns.sensor_id.size();
    if (n == 0) return out;

    // Row order sorted by (sensor_id, row). std::sort does not allocate; the row tie-break keeps
    // each group in input order.
    std::vector<std::size_t> order(n);  // allocation 1
    for (std::size_t i = 0; i < n; ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        const std::uint32_t sa = columns.sensor_id[a];
        const std::uint32_t sb = columns.sensor_id[b];
        return sa != sb ? sa < sb : a < b;
    });

    std::size_t groups = 0;
    for (std::size_t i = 0; i < n; ++groups) {
        std::size_t j = i + 1;
        while (j < n && columns.sensor_id[order[j]] == columns.sensor_id[order[i]]) ++j;
        i = j;
    }
    out.reserve(groups);  // allocation 2

    for (std::size_t i = 0; i < n;) {
        const std::uint32_t id = columns.sensor_id[order[i]];
        double sum = 0.0;
        double max = columns.value[order[i]];
        std::size_t count = 0;
        for (; i < n && columns.sensor_id[order[i]] == id; ++i) {
            const double v = columns.value[order[i]];
            sum += v;
            max = std::max(max, v);
            ++count;
        }
        out.push_back(SensorSummary{id, count, sum / static_cast<double>(count), max});
    }
    return out;
}

}  // namespace cppref::performance
