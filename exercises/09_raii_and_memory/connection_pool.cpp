#include "connection_pool.hpp"

#include <stdexcept>

namespace cppref::raii_and_memory {

std::string Connection::query(std::string_view sql) {
    (void)sql;
    throw std::logic_error("not implemented: Connection::query");
}

ConnectionPool::ConnectionPool(std::size_t max_size, Factory factory) {
    (void)max_size;
    (void)factory;
    throw std::logic_error("not implemented: ConnectionPool::ConnectionPool");
}

}  // namespace cppref::raii_and_memory
