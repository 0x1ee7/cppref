#include "batch.hpp"
#include "errors.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <exception>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace eh = cppref::error_handling;
using std::chrono::milliseconds;

namespace {

// True when `outer` carries a nested exception of type Inner.
template <typename Inner>
bool nested_is(const std::exception& outer) {
    const auto* ne = dynamic_cast<const std::nested_exception*>(&outer);
    if (ne == nullptr || ne->nested_ptr() == nullptr) {
        return false;
    }
    try {
        std::rethrow_exception(ne->nested_ptr());
    } catch (const Inner&) {
        return true;
    } catch (...) {
        return false;
    }
}

template <typename E>
std::exception_ptr capture(E&& e) {
    return std::make_exception_ptr(std::forward<E>(e));
}

}  // namespace

// --- Part 7: batches of named tasks with aggregate errors --------------------

TEST(Part7Batch, RunBatchReturnsResultsInTaskOrder) {
    const std::vector<eh::NamedTask<int>> tasks{
        {"a", [] { return 1; }},
        {"b", [] { return 2; }},
    };
    const auto results = eh::run_batch(tasks);
    EXPECT_EQ(results, (std::vector<std::pair<std::string, int>>{{"a", 1}, {"b", 2}}));
}

TEST(Part7Batch, RunBatchKeepsGoingAfterAFailure) {
    bool second_ran = false;
    const std::vector<eh::NamedTask<int>> tasks{
        {"a", []() -> int { throw std::runtime_error("first"); }},
        {"b", [&second_ran] {
             second_ran = true;
             return 2;
         }},
    };
    EXPECT_THROW((void)eh::run_batch(tasks), eh::BatchError);
    EXPECT_TRUE(second_ran);
}

TEST(Part7Batch, BatchErrorSummarisesFailuresInTaskOrder) {
    const std::vector<eh::NamedTask<int>> tasks{
        {"a", []() -> int { throw std::runtime_error("x"); }},
        {"b", [] { return 2; }},
        {"c", []() -> int { throw std::runtime_error("y"); }},
    };
    try {
        (void)eh::run_batch(tasks);
        FAIL() << "expected BatchError";
    } catch (const eh::BatchError& e) {
        EXPECT_EQ(std::string(e.what()), "2 of 3 tasks failed");
        ASSERT_EQ(e.failures().size(), 2u);
        EXPECT_EQ(e.failures()[0].name, "a");
        EXPECT_EQ(e.failures()[1].name, "c");
    }
}

TEST(Part7Batch, CatchAllCapturesNonStdExceptions) {
    const std::vector<eh::NamedTask<int>> tasks{
        {"odd", []() -> int { throw 7; }},
    };
    try {
        (void)eh::run_batch(tasks);
        FAIL() << "expected BatchError";
    } catch (const eh::BatchError& e) {
        ASSERT_EQ(e.failures().size(), 1u);
        EXPECT_EQ(eh::failure_message(e.failures()[0].error), "unknown exception");
    }
}

TEST(Part7Batch, DuplicateNamesAreRejectedBeforeAnyTaskRuns) {
    bool ran = false;
    const std::vector<eh::NamedTask<int>> tasks{
        {"a", [&ran] {
             ran = true;
             return 1;
         }},
        {"a", [] { return 2; }},
    };
    EXPECT_THROW((void)eh::run_batch(tasks), std::invalid_argument);
    EXPECT_FALSE(ran);
}

TEST(Part7Batch, FailureMessageReadsWhatOrNoError) {
    EXPECT_EQ(eh::failure_message(std::exception_ptr{}), "no error");
    EXPECT_EQ(eh::failure_message(capture(std::runtime_error("boom"))), "boom");
}

TEST(Part7Batch, TriageSplitsFailuresByRetryClassification) {
    const eh::BatchError err(
        2, 2,
        {
            eh::TaskFailure{"net", capture(std::system_error(std::make_error_code(std::errc::timed_out), "t"))},
            eh::TaskFailure{"bad", capture(std::runtime_error("nope"))},
        });
    const auto split = eh::triage(err);
    ASSERT_EQ(split.transient.size(), 1u);
    ASSERT_EQ(split.permanent.size(), 1u);
    EXPECT_EQ(split.transient[0].name, "net");
    EXPECT_EQ(split.permanent[0].name, "bad");
}

// --- Part 7 (cont.): retry with backoff --------------------------------------

TEST(Part7Retry, TransientClassificationCoversRetryableCodes) {
    EXPECT_TRUE(eh::is_transient(std::system_error(std::make_error_code(std::errc::timed_out), "t")));
    EXPECT_TRUE(eh::is_transient(std::system_error(std::make_error_code(std::errc::connection_refused), "c")));
    EXPECT_FALSE(eh::is_transient(std::system_error(std::make_error_code(std::errc::permission_denied), "p")));
    EXPECT_FALSE(eh::is_transient(std::runtime_error("plain")));
}

TEST(Part7Retry, NullExceptionPtrIsNotTransient) {
    EXPECT_FALSE(eh::is_transient(std::exception_ptr{}));
    EXPECT_TRUE(eh::is_transient(capture(std::system_error(std::make_error_code(std::errc::timed_out), "t"))));
}

TEST(Part7Retry, SucceedsAfterTransientFailuresWithDoublingSleeps) {
    int calls = 0;
    std::vector<milliseconds> sleeps;
    eh::RetryPolicy policy;
    policy.attempts = 3;
    policy.sleep = [&sleeps](milliseconds d) { sleeps.push_back(d); };

    const int result = eh::retry_call(
        [&calls]() -> int {
            if (++calls < 3) {
                throw std::system_error(std::make_error_code(std::errc::timed_out), "slow");
            }
            return 42;
        },
        policy);

    EXPECT_EQ(result, 42);
    EXPECT_EQ(calls, 3);
    EXPECT_EQ(sleeps, (std::vector<milliseconds>{milliseconds{100}, milliseconds{200}}));
}

TEST(Part7Retry, PermanentErrorIsRethrownWithoutSleeping) {
    int calls = 0;
    std::vector<milliseconds> sleeps;
    eh::RetryPolicy policy;
    policy.sleep = [&sleeps](milliseconds d) { sleeps.push_back(d); };

    EXPECT_THROW(eh::retry_call(
                     [&calls]() -> int {
                         ++calls;
                         throw std::runtime_error("bad input");
                     },
                     policy),
                 std::runtime_error);
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(sleeps.empty());
}

TEST(Part7Retry, ExhaustionThrowsRetryExhaustedNestingTheLastError) {
    int calls = 0;
    std::vector<milliseconds> sleeps;
    eh::RetryPolicy policy;
    policy.attempts = 3;
    policy.sleep = [&sleeps](milliseconds d) { sleeps.push_back(d); };

    try {
        (void)eh::retry_call(
            [&calls]() -> int {
                ++calls;
                throw std::system_error(std::make_error_code(std::errc::timed_out), "slow");
            },
            policy);
        FAIL() << "expected RetryExhausted";
    } catch (const eh::RetryExhausted& e) {
        EXPECT_EQ(e.attempts(), 3);
        EXPECT_NE(std::string(e.what()).find("gave up after 3 attempts"), std::string::npos);
        EXPECT_TRUE(nested_is<std::system_error>(e));
    }
    EXPECT_EQ(calls, 3);
    EXPECT_EQ(sleeps.size(), 2u);  // no sleep after the final attempt
}

TEST(Part7Retry, BackoffDoublesBetweenAttempts) {
    std::vector<milliseconds> sleeps;
    eh::RetryPolicy policy;
    policy.attempts = 4;
    policy.sleep = [&sleeps](milliseconds d) { sleeps.push_back(d); };

    EXPECT_THROW(eh::retry_call(
                     []() -> int {
                         throw std::system_error(std::make_error_code(std::errc::timed_out), "slow");
                     },
                     policy),
                 eh::RetryExhausted);
    EXPECT_EQ(sleeps, (std::vector<milliseconds>{milliseconds{100}, milliseconds{200}, milliseconds{400}}));
}

TEST(Part7Retry, NonPositiveAttemptsIsInvalidAndNeverCalls) {
    int calls = 0;
    eh::RetryPolicy policy;
    policy.attempts = 0;
    policy.sleep = [](milliseconds) {};

    EXPECT_THROW(eh::retry_call(
                     [&calls]() -> int {
                         ++calls;
                         return 1;
                     },
                     policy),
                 std::invalid_argument);
    EXPECT_EQ(calls, 0);
}

TEST(Part7Retry, CustomClassifierCanMakeAnErrorTransient) {
    int calls = 0;
    eh::RetryPolicy policy;
    policy.attempts = 2;
    policy.sleep = [](milliseconds) {};
    policy.classify = [](const std::exception&) { return true; };

    EXPECT_THROW(eh::retry_call(
                     [&calls]() -> int {
                         ++calls;
                         throw std::runtime_error("flaky");
                     },
                     policy),
                 eh::RetryExhausted);
    EXPECT_EQ(calls, 2);
}

TEST(Part7Retry, NonStdExceptionIsPermanent) {
    int calls = 0;
    eh::RetryPolicy policy;
    policy.sleep = [](milliseconds) {};

    EXPECT_THROW(eh::retry_call(
                     [&calls]() -> int {
                         ++calls;
                         throw 5;
                     },
                     policy),
                 int);
    EXPECT_EQ(calls, 1);
}
