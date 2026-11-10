// Part 5 · Readiness: named checks with timeouts, failures isolated, JSON report.
#pragma once

#include <chrono>
#include <cstddef>
#include <expected>
#include <functional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace cppref::deployment::health {

/// A check returns success, or an error message. It may also throw.
/// It receives a stop token that is requested when its timeout expires.
/// A check that ignores the token blocks run_all() until it returns.
using CheckFn = std::function<std::expected<void, std::string>(std::stop_token)>;

struct CheckReport {
    std::string name;
    bool ok = false;
    std::string detail;                   // empty when ok; the failure reason otherwise
    std::chrono::milliseconds elapsed{0};
};

class HealthRegistry {
public:
    /// Throws std::invalid_argument on a duplicate name or a non-positive timeout.
    void add(std::string name, CheckFn check, std::chrono::milliseconds timeout);

    [[nodiscard]] std::size_t size() const;

    /// Runs every check in registration order. Never throws because a check failed,
    /// threw, or timed out. Timeout detail: "timeout after <N>ms". Exception detail:
    /// "exception: <what()>".
    [[nodiscard]] std::vector<CheckReport> run_all() const;

private:
    struct Entry {
        std::string name;
        CheckFn check;
        std::chrono::milliseconds timeout;
    };
    std::vector<Entry> entries_;
};

/// True when every report is ok (vacuously true for an empty list).
[[nodiscard]] bool all_ok(const std::vector<CheckReport>& reports);

/// Escapes quotes, backslashes, and control characters for a JSON string body.
[[nodiscard]] std::string escape_json(std::string_view text);

/// {"status":"ready"|"not_ready","checks":{"<name>":"ok"|"error: <detail>", ...}}
[[nodiscard]] std::string to_json(const std::vector<CheckReport>& reports);

}  // namespace cppref::deployment::health
