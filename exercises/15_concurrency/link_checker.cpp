#include "link_checker.hpp"

#include <stdexcept>

namespace cppref::concurrency {

std::vector<LinkResult> check_links(std::span<const std::string> urls,
                                    const Fetch& fetch,
                                    std::stop_token stop,
                                    std::size_t max_workers) {
    (void)urls;
    (void)fetch;
    (void)stop;
    (void)max_workers;
    throw std::logic_error("not implemented: check_links");
}

}  // namespace cppref::concurrency
