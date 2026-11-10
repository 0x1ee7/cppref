#include "layout.hpp"

#include <stdexcept>

namespace cppref::performance {

SampleColumns to_columns(std::span<const Sample> /*samples*/) {
    throw std::logic_error("not implemented: to_columns");
}

std::vector<SensorSummary> summarize_by_sensor(const SampleColumns& /*columns*/) {
    throw std::logic_error("not implemented: summarize_by_sensor");
}

}  // namespace cppref::performance
