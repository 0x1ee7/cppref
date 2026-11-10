#include "connection_pool.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace cppref::raii_and_memory {

std::string Connection::query(std::string_view sql) {
    if (broken_) throw std::runtime_error("connection is broken");
    return std::to_string(id_) + ":" + std::string(sql);
}

namespace detail {

struct PoolState {
    std::size_t max_size = 0;
    ConnectionPool::Factory factory;
    // Reserved to max_size, so giving a connection back never reallocates (keeps ~Lease noexcept).
    std::vector<std::unique_ptr<Connection>> idle;
    std::size_t leased = 0;
};

}  // namespace detail

ConnectionPool::ConnectionPool(std::size_t max_size, Factory factory)
    : state_(std::make_shared<detail::PoolState>()) {
    if (max_size == 0) throw std::invalid_argument("ConnectionPool max_size must be >= 1");
    state_->max_size = max_size;
    state_->factory = std::move(factory);
    state_->idle.reserve(max_size);
}

std::optional<ConnectionPool::Lease> ConnectionPool::try_acquire() {
    detail::PoolState& s = *state_;
    std::unique_ptr<Connection> conn;
    if (!s.idle.empty()) {
        conn = std::move(s.idle.back());
        s.idle.pop_back();
    } else if (s.leased < s.max_size) {
        conn = s.factory();  // may throw: the pool is still unchanged
        if (!conn) throw std::runtime_error("connection factory returned null");
    } else {
        return std::nullopt;
    }
    ++s.leased;
    return Lease(state_, std::move(conn));
}

std::size_t ConnectionPool::idle_count() const noexcept {
    return state_->idle.size();
}

std::size_t ConnectionPool::live_count() const noexcept {
    return state_->idle.size() + state_->leased;
}

ConnectionPool::Lease::Lease(std::shared_ptr<detail::PoolState> state, std::unique_ptr<Connection> conn) noexcept
    : state_(std::move(state)), conn_(std::move(conn)) {}

ConnectionPool::Lease::Lease(Lease&& other) noexcept
    : state_(std::move(other.state_)), conn_(std::move(other.conn_)) {}

ConnectionPool::Lease& ConnectionPool::Lease::operator=(Lease&& other) noexcept {
    if (this != &other) {
        give_back();
        state_ = std::move(other.state_);
        conn_ = std::move(other.conn_);
    }
    return *this;
}

ConnectionPool::Lease::~Lease() {
    give_back();
}

void ConnectionPool::Lease::give_back() noexcept {
    if (!conn_) return;
    detail::PoolState& s = *state_;
    --s.leased;
    if (conn_->broken()) return;  // conn_ is destroyed with this lease
    s.idle.push_back(std::move(conn_));  // capacity reserved: no allocation
}

}  // namespace cppref::raii_and_memory
