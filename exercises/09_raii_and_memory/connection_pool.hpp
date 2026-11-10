#pragma once

#include <stdexcept>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace cppref::raii_and_memory {

// A fake database connection. Queries are recorded so tests can observe reuse.
class Connection {
public:
    explicit Connection(int id) : id_(id) {}

    int id() const noexcept { return id_; }
    // Returns "<id>:<sql>". Throws std::runtime_error when the connection is broken.
    std::string query(std::string_view sql);
    // Marks the connection unusable; a pool must discard it on release.
    void mark_broken() noexcept { broken_ = true; }
    bool broken() const noexcept { return broken_; }

private:
    int id_;
    bool broken_ = false;
};

namespace detail {
struct PoolState;
}

// Bounded pool. Lease is a move-only RAII handle: destroying it returns the connection to the
// pool, or discards it when broken. Leases keep the pool's shared state alive, so a Lease may
// outlive the ConnectionPool object; the last Lease (or the pool) to go away frees everything.
class ConnectionPool {
public:
    using Factory = std::function<std::unique_ptr<Connection>()>;

    class Lease {
    public:
        Lease(Lease&& other) noexcept { (void)other; }
        Lease& operator=(Lease&& other) noexcept {
            (void)other;
            return *this;
        }
        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;
        // Returns the connection to the pool (or discards it if broken). Never throws.
        ~Lease() {}

        Connection& operator*() const { throw std::logic_error("not implemented: Lease::operator*"); }
        Connection* operator->() const noexcept { return nullptr; }
        // False for a moved-from lease.
        explicit operator bool() const noexcept { return false; }

    private:
        friend class ConnectionPool;
        Lease() = default;
        // Engineer-owned: std::shared_ptr<detail::PoolState> state_; std::unique_ptr<Connection> conn_;
    };

    // max_size is the maximum number of live connections (idle plus leased). Must be >= 1.
    ConnectionPool(std::size_t max_size, Factory factory);

    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;
    ConnectionPool(ConnectionPool&&) = delete;
    ConnectionPool& operator=(ConnectionPool&&) = delete;
    ~ConnectionPool() {}

    // Reuses an idle connection, or creates one while under max_size. Returns nullopt when
    // every slot is leased. Propagates exceptions from the factory; the pool is then unchanged.
    std::optional<Lease> try_acquire() { return std::nullopt; }

    std::size_t idle_count() const noexcept { return 0; }
    // Idle plus leased connections.
    std::size_t live_count() const noexcept { return 0; }

private:
    // Engineer-owned: std::shared_ptr<detail::PoolState> state_.
};

}  // namespace cppref::raii_and_memory
