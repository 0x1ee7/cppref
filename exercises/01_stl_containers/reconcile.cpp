#include "reconcile.hpp"

#include <stdexcept>

namespace cppref::stl {

bool Reconciliation::is_clean() const {
    throw std::logic_error("not implemented: Reconciliation::is_clean");
}

Reconciliation reconcile(const std::map<std::string, std::int64_t>&,
                         const std::map<std::string, std::int64_t>&) {
    throw std::logic_error("not implemented: reconcile");
}

}  // namespace cppref::stl
