#include "transaction.hpp"

#include <stdexcept>

namespace cppref::raii_and_memory {

Transaction::Transaction(KvMap& target) {
    (void)target;
    throw std::logic_error("not implemented: Transaction::Transaction");
}

}  // namespace cppref::raii_and_memory
