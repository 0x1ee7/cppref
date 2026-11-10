#include "downloader.hpp"
#include "test_support_test.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using cppref::concurrency::BoundedDownloader;
using cppref::concurrency::Fetcher;
using cppref::concurrency::testing::parallel_for;

namespace {

Fetcher echo_fetcher() {
    return [](std::string_view url) { return "body:" + std::string(url); };
}

// Sleeps briefly so that concurrent calls actually overlap.
Fetcher tracking_fetcher(std::atomic<std::size_t>& current, std::atomic<std::size_t>& peak) {
    return [&current, &peak](std::string_view url) {
        const auto now = ++current;
        auto seen = peak.load();
        while (now > seen && !peak.compare_exchange_weak(seen, now)) {
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        --current;
        return "body:" + std::string(url);
    };
}

}  // namespace

TEST(Part2Basics, DownloadReturnsFetchedBytes) {
    BoundedDownloader downloader(echo_fetcher(), 2);
    EXPECT_EQ(downloader.download("https://a.test/x"), "body:https://a.test/x");
}

TEST(Part2Basics, ZeroSlotsRejected) {
    EXPECT_THROW(BoundedDownloader(echo_fetcher(), 0), std::invalid_argument);
}

TEST(Part2Concurrency, PeakConcurrencyNeverExceedsLimit) {
    std::atomic<std::size_t> current{0};
    std::atomic<std::size_t> peak{0};
    BoundedDownloader downloader(tracking_fetcher(current, peak), 2);

    std::vector<std::string> urls;
    for (int i = 0; i < 10; ++i) urls.push_back("https://site.test/" + std::to_string(i));

    auto bodies = downloader.download_all(urls);
    EXPECT_EQ(bodies.size(), urls.size());
    EXPECT_GE(downloader.peak_concurrency(), 1u);
    EXPECT_LE(downloader.peak_concurrency(), 2u);
    EXPECT_LE(peak.load(), 2u);
}

TEST(Part2Concurrency, SlotsAreSharedAcrossCallers) {
    std::atomic<std::size_t> current{0};
    std::atomic<std::size_t> peak{0};
    BoundedDownloader downloader(tracking_fetcher(current, peak), 2);

    parallel_for(4, [&](std::size_t caller) {
        for (int i = 0; i < 3; ++i) {
            const std::string url = "https://site.test/" + std::to_string(caller) + "/" + std::to_string(i);
            EXPECT_EQ(downloader.download(url), "body:" + url);
        }
    });

    EXPECT_LE(peak.load(), 2u);
    EXPECT_EQ(downloader.in_flight(), 0u);
}

TEST(Part2Concurrency, DownloadAllKeepsOrderAndHandlesEmpty) {
    BoundedDownloader downloader(echo_fetcher(), 3);

    const std::vector<std::string> urls{"u0", "u1", "u2", "u3", "u4"};
    auto bodies = downloader.download_all(urls);
    ASSERT_EQ(bodies.size(), urls.size());
    for (std::size_t i = 0; i < urls.size(); ++i) EXPECT_EQ(bodies[i], "body:" + urls[i]);

    const std::vector<std::string> none;
    EXPECT_TRUE(downloader.download_all(none).empty());
}

TEST(Part2Failures, EarliestFailureIsRethrownAfterAllFinish) {
    std::atomic<int> attempts{0};
    Fetcher fetch = [&](std::string_view url) -> std::string {
        ++attempts;
        if (url.starts_with("bad")) throw std::runtime_error(std::string(url));
        return "ok";
    };
    BoundedDownloader downloader(fetch, 2);

    const std::vector<std::string> urls{"good", "bad-1", "good-2", "bad-3"};
    try {
        downloader.download_all(urls);
        FAIL() << "expected download_all to throw";
    } catch (const std::runtime_error& e) {
        EXPECT_EQ(std::string(e.what()), "bad-1");
    }
    EXPECT_EQ(attempts.load(), 4);
}

TEST(Part2Failures, SlotIsReleasedWhenFetchThrows) {
    std::atomic<bool> fail_next{true};
    Fetcher fetch = [&](std::string_view url) -> std::string {
        if (fail_next.exchange(false)) throw std::runtime_error("transient");
        return "body:" + std::string(url);
    };
    BoundedDownloader downloader(fetch, 1);

    EXPECT_THROW(downloader.download("first"), std::runtime_error);
    // With a single slot, a leaked slot would make this call block forever.
    auto future = std::async(std::launch::async, [&] { return downloader.download("second"); });
    ASSERT_EQ(future.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    EXPECT_EQ(future.get(), "body:second");
    EXPECT_EQ(downloader.in_flight(), 0u);
}
