#include "health.hpp"

#include <stdexcept>

namespace cppref::deployment::health {

void HealthRegistry::add(std::string, CheckFn, std::chrono::milliseconds) {
    throw std::logic_error("not implemented: HealthRegistry::add");
}

std::size_t HealthRegistry::size() const {
    throw std::logic_error("not implemented: HealthRegistry::size");
}

std::vector<CheckReport> HealthRegistry::run_all() const {
    throw std::logic_error("not implemented: HealthRegistry::run_all");
}

bool all_ok(const std::vector<CheckReport>&) {
    throw std::logic_error("not implemented: all_ok");
}

std::string escape_json(std::string_view) {
    throw std::logic_error("not implemented: escape_json");
}

std::string to_json(const std::vector<CheckReport>&) {
    throw std::logic_error("not implemented: to_json");
}

}  // namespace cppref::deployment::health
