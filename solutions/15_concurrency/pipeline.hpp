#pragma once

#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace cppref::concurrency {

using Stage = std::function<std::string(std::string)>;

// Runs inputs through stages in order, as a multi-stage pipeline connected by BoundedQueues.
//
// - Each stage has `workers_per_stage` threads; consecutive stages are joined by a queue of
//   `queue_capacity` items, so a slow stage applies backpressure upstream.
// - results[i] is the output of inputs[i] after every stage (order is preserved in the result
//   even though items can finish out of order inside the pipeline).
// - With no stages, the output equals the input. With no inputs, the result is empty.
// - If any stage throws, the pipeline is torn down (every thread joined) and the first exception
//   is rethrown. Partial results are discarded.
// - Throws std::invalid_argument if workers_per_stage == 0, queue_capacity == 0, or a stage is empty.
std::vector<std::string> run_pipeline(std::span<const std::string> inputs,
                                      std::span<const Stage> stages,
                                      std::size_t workers_per_stage = 2,
                                      std::size_t queue_capacity = 4);

}  // namespace cppref::concurrency
