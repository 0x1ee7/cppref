#include "shutdown.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <stdexcept>
#include <string>
#include <thread>

namespace cppref::deployment {
namespace {

std::atomic<bool> g_active{false};
std::atomic<int> g_pending{0};
static_assert(std::atomic<int>::is_always_lock_free);

// Async-signal-safe: a single lock-free compare-exchange, keeping only the first signal.
void on_signal(int sig) {
    int expected = 0;
    g_pending.compare_exchange_strong(expected, sig);
}

}  // namespace

SignalGuard::SignalGuard(std::initializer_list<int> signals) {
    bool expected = false;
    if (!g_active.compare_exchange_strong(expected, true)) {
        throw std::logic_error("another SignalGuard is already active");
    }
    g_pending.store(0);

    try {
        for (int sig : signals) {
            auto prev = std::signal(sig, on_signal);
            if (prev == SIG_ERR) {
                throw std::invalid_argument("cannot install handler for signal " + std::to_string(sig));
            }
            previous_.emplace_back(sig, prev);
        }
    } catch (...) {
        for (auto it = previous_.rbegin(); it != previous_.rend(); ++it) std::signal(it->first, it->second);
        g_active.store(false);
        throw;
    }

    watcher_ = std::jthread([this](std::stop_token st) {
        while (!st.stop_requested()) {
            if (const int sig = g_pending.load(); sig != 0) {
                received_.store(sig);
                stop_.request_stop();
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
}

SignalGuard::~SignalGuard() {
    stop_.request_stop();
    for (auto it = previous_.rbegin(); it != previous_.rend(); ++it) std::signal(it->first, it->second);
    g_active.store(false);
}

std::stop_token SignalGuard::token() const { return stop_.get_token(); }

std::optional<int> SignalGuard::received_signal() const {
    if (const int sig = received_.load(); sig != 0) return sig;
    return std::nullopt;
}

}  // namespace cppref::deployment
