#pragma once

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
    // Returns "<id>:<sql>". Throws std::runtime_error when broken.
    std::string query(std::string_view sql);
    void mark_broken() noexcept { broken_ = true; }
    bool broken() const noexcept { return broken_; }

private:
    int id_;
    bool broken_ = false;
};

namespace detail {
struct PoolState;
}

// Bounded pool with RAII leases. Leases share ownership of the pool state, so a Lease may
// outlive the ConnectionPool object.
class ConnectionPool {
public:
    using Factory = std::function<std::unique_ptr<Connection>()>;

    class Lease {
    public:
        Lease(Lease&& other) noexcept;
        Lease& operator=(Lease&& other) noexcept;
        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;
        ~Lease();

        // Precondition: the lease is engaged (operator bool is true).
        Connection& operator*() const noexcept { return *conn_; }
        Connection* operator->() const noexcept { return conn_.get(); }
        explicit operator bool() const noexcept { return conn_ != nullptr; }

    private:
        friend class ConnectionPool;
        Lease(std::shared_ptr<detail::PoolState> state, std::unique_ptr<Connection> conn) noexcept;
        void give_back() noexcept;

        std::shared_ptr<detail::PoolState> state_;
        std::unique_ptr<Connection> conn_;
    };

    // max_size >= 1; throws std::invalid_argument otherwise.
    ConnectionPool(std::size_t max_size, Factory factory);

    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;
    ConnectionPool(ConnectionPool&&) = delete;
    ConnectionPool& operator=(ConnectionPool&&) = delete;
    ~ConnectionPool() = default;

    std::optional<Lease> try_acquire();

    std::size_t idle_count() const noexcept;
    std::size_t live_count() const noexcept;

private:
    std::shared_ptr<detail::PoolState> state_;
};

}  // namespace cppref::raii_and_memory
