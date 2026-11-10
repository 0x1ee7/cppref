#include "health.hpp"

#include <algorithm>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace cppref::deployment::health {
namespace {

struct Outcome {
    std::mutex mutex;
    std::condition_variable cv;
    bool done = false;
    std::expected<void, std::string> result;
};

CheckReport run_one(const std::string& name, const CheckFn& check, std::chrono::milliseconds timeout) {
    const auto start = std::chrono::steady_clock::now();
    // Shared state: the worker may outlive a timed-out wait, so it must not reference our stack.
    auto state = std::make_shared<Outcome>();

    std::jthread worker([state, check](std::stop_token st) {
        std::expected<void, std::string> result;
        try {
            result = check(st);
        } catch (const std::exception& e) {
            result = std::unexpected(std::string("exception: ") + e.what());
        } catch (...) {
            result = std::unexpected(std::string("exception: unknown"));
        }
        {
            std::lock_guard lock(state->mutex);
            state->result = std::move(result);
            state->done = true;
        }
        state->cv.notify_all();
    });

    bool finished = false;
    {
        std::unique_lock lock(state->mutex);
        finished = state->cv.wait_for(lock, timeout, [&] { return state->done; });
    }
    if (!finished) worker.request_stop();
    worker.join();

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    if (!finished) {
        return {name, false, "timeout after " + std::to_string(timeout.count()) + "ms", elapsed};
    }
    if (!state->result) return {name, false, state->result.error(), elapsed};
    return {name, true, "", elapsed};
}

}  // namespace

void HealthRegistry::add(std::string name, CheckFn check, std::chrono::milliseconds timeout) {
    if (timeout <= std::chrono::milliseconds::zero()) {
        throw std::invalid_argument("timeout must be positive: " + name);
    }
    const bool duplicate = std::ranges::any_of(entries_, [&](const Entry& e) { return e.name == name; });
    if (duplicate) throw std::invalid_argument("duplicate health check: " + name);
    entries_.push_back(Entry{std::move(name), std::move(check), timeout});
}

std::size_t HealthRegistry::size() const { return entries_.size(); }

std::vector<CheckReport> HealthRegistry::run_all() const {
    std::vector<CheckReport> reports;
    reports.reserve(entries_.size());
    for (const auto& entry : entries_) {
        reports.push_back(run_one(entry.name, entry.check, entry.timeout));
    }
    return reports;
}

bool all_ok(const std::vector<CheckReport>& reports) {
    return std::ranges::all_of(reports, [](const CheckReport& r) { return r.ok; });
}

std::string escape_json(std::string_view text) {
    constexpr std::string_view hex = "0123456789abcdef";
    std::string out;
    out.reserve(text.size());
    for (unsigned char c : text) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    out += "\\u00";
                    out += hex[c >> 4];
                    out += hex[c & 0x0F];
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

std::string to_json(const std::vector<CheckReport>& reports) {
    std::string out = std::string("{\"status\":\"") + (all_ok(reports) ? "ready" : "not_ready") + "\",\"checks\":{";
    bool first = true;
    for (const auto& r : reports) {
        if (!first) out += ',';
        first = false;
        const std::string value = r.ok ? std::string("ok") : "error: " + r.detail;
        out += "\"" + escape_json(r.name) + "\":\"" + escape_json(value) + "\"";
    }
    out += "}}";
    return out;
}

}  // namespace cppref::deployment::health
