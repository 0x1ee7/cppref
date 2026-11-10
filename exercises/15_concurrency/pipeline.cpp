#include "pipeline.hpp"

#include <stdexcept>

namespace cppref::concurrency {

std::vector<std::string> run_pipeline(std::span<const std::string> inputs,
                                      std::span<const Stage> stages,
                                      std::size_t workers_per_stage,
                                      std::size_t queue_capacity) {
    (void)inputs;
    (void)stages;
    (void)workers_per_stage;
    (void)queue_capacity;
    throw std::logic_error("not implemented: run_pipeline");
}

}  // namespace cppref::concurrency
