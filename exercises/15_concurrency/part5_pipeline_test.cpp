#include "pipeline.hpp"

#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using cppref::concurrency::run_pipeline;
using cppref::concurrency::Stage;

namespace {

Stage append(std::string suffix) {
    return [suffix](std::string s) { return s + suffix; };
}

}  // namespace

TEST(Part5Pipeline, NoStagesReturnsInputsUnchanged) {
    const std::vector<std::string> inputs{"x", "y", "z"};
    const std::vector<Stage> stages;
    EXPECT_EQ(run_pipeline(inputs, stages), inputs);
}

TEST(Part5Pipeline, NoInputReturnsEmpty) {
    const std::vector<std::string> inputs;
    const std::vector<Stage> stages{append("!")};
    EXPECT_TRUE(run_pipeline(inputs, stages).empty());
}

TEST(Part5Pipeline, AppliesStagesInOrder) {
    const std::vector<std::string> inputs{"x", "y"};
    const std::vector<Stage> stages{append("a"), append("b")};
    const std::vector<std::string> expected{"xab", "yab"};
    EXPECT_EQ(run_pipeline(inputs, stages), expected);
}

TEST(Part5Pipeline, PreservesOrderWhenItemsFinishOutOfOrder) {
    std::vector<std::string> inputs;
    for (int i = 0; i < 10; ++i) inputs.push_back(std::to_string(i));

    // Items ending in '0' are slow, so later items overtake them inside the pipeline.
    const std::vector<Stage> stages{
        [](std::string s) {
            if (s.back() == '0') std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return s + "!";
        },
        append("?"),
    };

    const auto results = run_pipeline(inputs, stages, 4, 2);
    ASSERT_EQ(results.size(), inputs.size());
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        EXPECT_EQ(results[i], inputs[i] + "!?");
    }
}

TEST(Part5Pipeline, StageFailureTearsDownAndRethrows) {
    std::vector<std::string> inputs;
    for (int i = 0; i < 20; ++i) inputs.push_back("item" + std::to_string(i));

    const std::vector<Stage> stages{
        [](std::string s) -> std::string {
            if (s == "item7") throw std::runtime_error("stage boom");
            return s;
        },
        append("!"),
    };

    try {
        (void)run_pipeline(inputs, stages);
        FAIL() << "expected run_pipeline to throw";
    } catch (const std::runtime_error& e) {
        EXPECT_EQ(std::string(e.what()), "stage boom");
    }
}

TEST(Part5Pipeline, LargeRunKeepsEveryItem) {
    std::vector<std::string> inputs;
    for (int i = 0; i < 500; ++i) inputs.push_back(std::to_string(i));
    const std::vector<Stage> stages{append("#"), append("$")};

    const auto results = run_pipeline(inputs, stages, 3, 2);
    ASSERT_EQ(results.size(), inputs.size());
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        EXPECT_EQ(results[i], inputs[i] + "#$");
    }
}

TEST(Part5Pipeline, BadArgumentsRejected) {
    const std::vector<std::string> inputs{"x"};
    const std::vector<Stage> stages{append("!")};
    const std::vector<Stage> with_empty{Stage{}};

    EXPECT_THROW(run_pipeline(inputs, stages, 0, 4), std::invalid_argument);
    EXPECT_THROW(run_pipeline(inputs, stages, 2, 0), std::invalid_argument);
    EXPECT_THROW(run_pipeline(inputs, with_empty), std::invalid_argument);
}
