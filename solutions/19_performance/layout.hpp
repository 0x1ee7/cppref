// Part 6: data layout. Field order decides sizeof; columnar (SoA) storage decides how much of a
// cache line each pass touches and how many allocations an aggregation makes.
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace cppref::performance {

// Members ordered by alignment, largest first: no interior padding, only 2 tail bytes.
// sizeof(Sample) == 24 on x86-64 MSVC/GCC/Clang.
struct Sample {
    double value;
    std::int64_t timestamp_ns;
    std::uint32_t sensor_id;
    std::uint8_t kind;
    std::uint8_t flags;
};

// Struct-of-arrays view of a batch of samples: one contiguous vector per column.
struct SampleColumns {
    std::vector<std::int64_t> timestamp_ns;
    std::vector<double> value;
    std::vector<std::uint32_t> sensor_id;
};

// Splits samples into columns, preserving order. Reserves each column once: at most three
// allocations for any input size. Drops kind and flags (not needed by the aggregation).
SampleColumns to_columns(std::span<const Sample> samples);

struct SensorSummary {
    std::uint32_t sensor_id;
    std::size_t count;
    double mean;
    double max;
};

// One summary per distinct sensor_id, sorted by sensor_id ascending. Works only on the columns.
// Empty input gives an empty result. Allocation budget: at most three allocations regardless of
// the sample count (no per-sample allocation).
std::vector<SensorSummary> summarize_by_sensor(const SampleColumns& columns);

}  // namespace cppref::performance
