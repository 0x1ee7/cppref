// Part 5: batched writes and resource-owning read_batches. Part 6: moving average.
#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "batching.hpp"

namespace ir = cppref::iterators_and_ranges;
namespace views = std::ranges::views;

namespace {

// Stream that counts its own destruction, so tests can see when the source is released.
class TrackedStream : public std::istringstream {
public:
    TrackedStream(std::string text, int* destroyed)
        : std::istringstream(std::move(text)), destroyed_(destroyed) {}
    ~TrackedStream() { ++*destroyed_; }

private:
    int* destroyed_;
};

ir::SourceOpener opener_for(std::string text, int* opens, int* destroyed) {
    return [text = std::move(text), opens, destroyed]() -> std::unique_ptr<std::istream> {
        ++*opens;
        return std::make_unique<TrackedStream>(text, destroyed);
    };
}

template <typename G>
std::vector<double> drain_doubles(G&& gen) {
    std::vector<double> out;
    for (double v : gen) {
        out.push_back(v);
    }
    return out;
}

}  // namespace

TEST(Part5WriteInBatches, SplitsIntoBatchesOfSize) {
    std::vector<std::vector<int>> seen;
    auto total = ir::write_in_batches(std::vector<int>{1, 2, 3, 4, 5, 6, 7}, 3,
                                      [&seen](std::vector<int> batch) {
                                          auto n = batch.size();
                                          seen.push_back(std::move(batch));
                                          return n;
                                      });
    EXPECT_EQ(seen, (std::vector<std::vector<int>>{{1, 2, 3}, {4, 5, 6}, {7}}));
    EXPECT_EQ(total, 7U);
}

TEST(Part5WriteInBatches, ReturnsSumOfWriteResults) {
    auto total = ir::write_in_batches(std::vector<int>{1, 2, 3, 4}, 2,
                                      [](std::vector<int> batch) {
                                          std::size_t sum = 0;
                                          for (int v : batch) {
                                              sum += static_cast<std::size_t>(v);
                                          }
                                          return sum;
                                      });
    EXPECT_EQ(total, 10U);  // (1+2) + (3+4)
}

TEST(Part5WriteInBatches, PullsOnlyWhatTheCurrentBatchNeeds) {
    int pulled = 0;
    auto source = views::iota(1) | views::transform([&pulled](int v) {
                      ++pulled;
                      return v;
                  });
    int calls = 0;
    EXPECT_THROW((void)ir::write_in_batches(source, 3,
                                            [&calls](std::vector<int>) -> std::size_t {
                                                if (++calls == 2) {
                                                    throw std::runtime_error("disk full");
                                                }
                                                return 0;
                                            }),
                 std::runtime_error);
    EXPECT_EQ(calls, 2);
    EXPECT_EQ(pulled, 6);  // two full batches of 3, no read-ahead
}

TEST(Part5WriteInBatches, ZeroBatchSizeThrowsBeforePulling) {
    int pulled = 0;
    std::vector<int> values{1, 2, 3};
    auto source = values | views::transform([&pulled](int v) {
                      ++pulled;
                      return v;
                  });
    EXPECT_THROW((void)ir::write_in_batches(source, 0, [](std::vector<int>) -> std::size_t {
                     return 0;
                 }),
                 std::invalid_argument);
    EXPECT_EQ(pulled, 0);
}

TEST(Part5ReadBatches, GroupsLinesIntoBatches) {
    int opens = 0;
    int destroyed = 0;
    std::vector<std::vector<std::string>> out;
    for (auto& batch : ir::read_batches(opener_for("a\nb\nc\nd\ne\n", &opens, &destroyed), 2)) {
        out.push_back(batch);
    }
    EXPECT_EQ(out, (std::vector<std::vector<std::string>>{{"a", "b"}, {"c", "d"}, {"e"}}));
}

TEST(Part5ReadBatches, OpensSourceOnFirstPullNotOnCall) {
    int opens = 0;
    int destroyed = 0;
    auto gen = ir::read_batches(opener_for("a\nb\n", &opens, &destroyed), 1);
    EXPECT_EQ(opens, 0);
    auto it = gen.begin();
    EXPECT_EQ(opens, 1);
    EXPECT_EQ((*it).size(), 1U);
}

TEST(Part5ReadBatches, ReleasesSourceWhenExhausted) {
    int opens = 0;
    int destroyed = 0;
    auto gen = ir::read_batches(opener_for("a\nb\nc\n", &opens, &destroyed), 2);
    for (auto& batch : gen) {
        (void)batch;
    }
    EXPECT_EQ(destroyed, 1);
}

TEST(Part5ReadBatches, ReleasesSourceOnEarlyBreak) {
    int opens = 0;
    int destroyed = 0;
    {
        auto gen = ir::read_batches(opener_for("a\nb\nc\n", &opens, &destroyed), 1);
        for (auto& batch : gen) {
            (void)batch;
            break;
        }
        EXPECT_EQ(destroyed, 0);  // still open while the generator is alive
    }
    EXPECT_EQ(destroyed, 1);
}

TEST(Part5ReadBatches, ZeroBatchSizeThrowsImmediately) {
    int opens = 0;
    int destroyed = 0;
    EXPECT_THROW((void)ir::read_batches(opener_for("a\n", &opens, &destroyed), 0),
                 std::invalid_argument);
    EXPECT_EQ(opens, 0);
}

TEST(Part6MovingAverage, AveragesEachFullWindow) {
    EXPECT_EQ(drain_doubles(ir::moving_average(std::vector<double>{1, 2, 3, 4, 5}, 3)),
              (std::vector<double>{2.0, 3.0, 4.0}));
}

TEST(Part6MovingAverage, NeverEmitsPartialWindows) {
    EXPECT_EQ(drain_doubles(ir::moving_average(std::vector<double>{1, 2, 3, 4, 5}, 4)),
              (std::vector<double>{2.5, 3.5}));
    EXPECT_EQ(drain_doubles(ir::moving_average(std::vector<double>{1, 2}, 3)),
              (std::vector<double>{}));
}

TEST(Part6MovingAverage, ZeroWindowThrowsEagerly) {
    EXPECT_THROW((void)ir::moving_average(std::vector<double>{1.0}, 0), std::invalid_argument);
}

namespace {

ir::Generator<double> averages_of_local() {
    std::vector<double> local{2.0, 4.0, 6.0};
    return ir::moving_average(local, 2);  // the result owns a copy of the input
}

}  // namespace

TEST(Part6MovingAverage, OwnsItsInputAndOutlivesTheCaller) {
    auto gen = averages_of_local();
    EXPECT_EQ(drain_doubles(gen), (std::vector<double>{3.0, 5.0}));
}

TEST(Part6MovingAverage, IsLazyAndPullsOnlyWhatTheWindowNeeds) {
    int pulled = 0;
    auto source = views::iota(1) | views::transform([&pulled](int v) {
                      ++pulled;
                      return v;
                  });
    auto gen = ir::moving_average(source, 2);
    EXPECT_EQ(pulled, 0);
    auto it = gen.begin();
    EXPECT_EQ(*it, 1.5);
    EXPECT_EQ(pulled, 2);
}
