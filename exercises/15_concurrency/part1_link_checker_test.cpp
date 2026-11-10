#include "link_checker.hpp"
#include "test_support_test.hpp"

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using cppref::concurrency::check_links;
using cppref::concurrency::Fetch;
using cppref::concurrency::LinkResult;

namespace {

Fetch always_ok() {
    return [](std::string_view, std::stop_token) { return 200; };
}

}  // namespace

TEST(Part1Basics, ResultsKeepInputOrderAndStatus) {
    const std::vector<std::string> urls{"https://a.test/", "https://b.test/", "https://c.test/"};
    auto results = check_links(urls, always_ok(), std::stop_token{});

    ASSERT_EQ(results.size(), urls.size());
    for (std::size_t i = 0; i < urls.size(); ++i) {
        EXPECT_EQ(results[i].url, urls[i]);
        EXPECT_EQ(results[i].status, 200);
        EXPECT_FALSE(results[i].error.has_value());
        EXPECT_TRUE(results[i].ok());
    }
}

TEST(Part1Basics, ErrorStatusIsNotOk) {
    const std::vector<std::string> urls{"https://a.test/missing"};
    Fetch fetch = [](std::string_view, std::stop_token) { return 404; };
    auto results = check_links(urls, fetch, std::stop_token{});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].status, 404);
    EXPECT_FALSE(results[0].ok());
}

TEST(Part1Basics, FetchExceptionBecomesErrorText) {
    const std::vector<std::string> urls{"https://a.test/", "https://b.test/"};
    Fetch fetch = [](std::string_view url, std::stop_token) -> int {
        if (url == "https://b.test/") throw std::runtime_error("connection reset");
        return 200;
    };
    auto results = check_links(urls, fetch, std::stop_token{});

    ASSERT_EQ(results.size(), 2u);
    EXPECT_TRUE(results[0].ok());
    EXPECT_FALSE(results[1].status.has_value());
    ASSERT_TRUE(results[1].error.has_value());
    EXPECT_EQ(*results[1].error, "connection reset");
}

TEST(Part1Basics, EmptyInputReturnsEmpty) {
    const std::vector<std::string> urls;
    EXPECT_TRUE(check_links(urls, always_ok(), std::stop_token{}).empty());
}

TEST(Part1Basics, ZeroWorkersRejected) {
    const std::vector<std::string> urls{"https://a.test/"};
    EXPECT_THROW(check_links(urls, always_ok(), std::stop_token{}, 0), std::invalid_argument);
}

TEST(Part1Concurrency, NeverExceedsWorkerLimit) {
    std::vector<std::string> urls;
    for (int i = 0; i < 12; ++i) urls.push_back("https://site.test/" + std::to_string(i));

    std::atomic<int> current{0};
    std::atomic<int> peak{0};
    Fetch fetch = [&](std::string_view, std::stop_token) {
        const int now = ++current;
        int seen = peak.load();
        while (now > seen && !peak.compare_exchange_weak(seen, now)) {
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        --current;
        return 200;
    };

    auto results = check_links(urls, fetch, std::stop_token{}, 3);
    EXPECT_EQ(results.size(), urls.size());
    EXPECT_GE(peak.load(), 1);
    EXPECT_LE(peak.load(), 3);
}

TEST(Part1Concurrency, DuplicateUrlsAreFetchedOnce) {
    const std::vector<std::string> urls{"https://a.test/", "https://a.test/", "https://b.test/"};
    std::atomic<int> a_fetches{0};
    Fetch fetch = [&](std::string_view url, std::stop_token) {
        if (url == "https://a.test/") ++a_fetches;
        return 200;
    };

    auto results = check_links(urls, fetch, std::stop_token{});
    EXPECT_EQ(a_fetches.load(), 1);
    ASSERT_EQ(results.size(), 3u);
    EXPECT_EQ(results[0], results[1]);
    EXPECT_EQ(results[0].url, "https://a.test/");
}

TEST(Part1Cancellation, StopBeforeStartCancelsEverything) {
    const std::vector<std::string> urls{"https://a.test/", "https://b.test/"};
    std::atomic<int> fetches{0};
    Fetch fetch = [&](std::string_view, std::stop_token) {
        ++fetches;
        return 200;
    };

    std::stop_source source;
    source.request_stop();
    auto results = check_links(urls, fetch, source.get_token());

    EXPECT_EQ(fetches.load(), 0);
    ASSERT_EQ(results.size(), 2u);
    for (const auto& r : results) {
        ASSERT_TRUE(r.error.has_value());
        EXPECT_EQ(*r.error, "cancelled");
        EXPECT_FALSE(r.status.has_value());
    }
}

TEST(Part1Cancellation, StopMidRunCancelsRemainingUrls) {
    const std::vector<std::string> urls{"https://a.test/", "https://b.test/", "https://c.test/"};
    std::stop_source source;
    std::atomic<int> fetches{0};
    Fetch fetch = [&](std::string_view, std::stop_token) {
        if (fetches++ == 0) source.request_stop();
        return 200;
    };

    // One worker makes the order deterministic: the first fetch requests stop.
    auto results = check_links(urls, fetch, source.get_token(), 1);

    EXPECT_EQ(fetches.load(), 1);
    ASSERT_EQ(results.size(), 3u);
    EXPECT_TRUE(results[0].ok());
    for (std::size_t i = 1; i < results.size(); ++i) {
        ASSERT_TRUE(results[i].error.has_value());
        EXPECT_EQ(*results[i].error, "cancelled");
    }
}
