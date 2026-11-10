#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace cppref::concurrency {

// Outcome of checking one URL. Exactly one of `status` / `error` is set.
struct LinkResult {
    std::string url;
    std::optional<int> status;   // HTTP-style status code when the fetch completed
    std::optional<std::string> error;  // "cancelled" or the fetch exception message

    bool ok() const { return status.has_value() && *status >= 200 && *status < 400; }
    bool operator==(const LinkResult&) const = default;
};

// Blocking fetch: returns a status code, or throws on transport failure.
using Fetch = std::function<int(std::string_view url, std::stop_token)>;

// Checks every URL with at most `max_workers` concurrent fetches.
// - Results are returned in input order, one per input URL (duplicates are fetched once and
//   share the same result, but each input position gets its own entry).
// - A fetch exception becomes `error` with the exception's what() text.
// - Once `stop` is requested, URLs not yet started get error "cancelled"; in-flight fetches
//   are awaited (the function never returns while a worker still runs).
// - Throws std::invalid_argument if max_workers == 0.
std::vector<LinkResult> check_links(std::span<const std::string> urls,
                                    const Fetch& fetch,
                                    std::stop_token stop,
                                    std::size_t max_workers = 8);

}  // namespace cppref::concurrency
