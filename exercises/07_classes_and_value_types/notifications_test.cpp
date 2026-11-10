// Part 7: Notifier interface, concrete channels and NotificationDispatcher.
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "notifications.hpp"

namespace cc = cppref::classes;

using ::testing::Return;
using ::testing::Throw;

namespace {

using Email = testing::MockFunction<void(std::string_view, std::string_view)>;
using Post = testing::MockFunction<int(std::string_view, std::string_view)>;

std::vector<std::unique_ptr<cc::Notifier>> notifiers_of(std::unique_ptr<cc::Notifier> a,
                                                        std::unique_ptr<cc::Notifier> b = nullptr) {
    std::vector<std::unique_ptr<cc::Notifier>> out;
    out.push_back(std::move(a));
    if (b) {
        out.push_back(std::move(b));
    }
    return out;
}

}  // namespace

TEST(Part7Notifier, IsAbstractAndDescribesItsChannel) {
    EXPECT_TRUE(std::is_abstract_v<cc::Notifier>);
    cc::EmailNotifier email([](std::string_view, std::string_view) {});
    EXPECT_EQ(email.describe(), "email notifier");
}

TEST(Part7Notifier, ChannelNamesAreStable) {
    cc::EmailNotifier email([](std::string_view, std::string_view) {});
    cc::SMSNotifier sms([](std::string_view, std::string_view) {});
    cc::WebhookNotifier hook("https://hooks.example/a", [](std::string_view, std::string_view) { return 200; });
    EXPECT_EQ(email.channel_name(), "email");
    EXPECT_EQ(sms.channel_name(), "sms");
    EXPECT_EQ(hook.channel_name(), "webhook");
}

TEST(Part7Email, SendForwardsToTransportAndRejectsBadAddress) {
    Email transport;
    EXPECT_CALL(transport, Call(std::string_view{"ada@example.com"}, std::string_view{"hi"})).Times(1);
    cc::EmailNotifier email(transport.AsStdFunction());
    email.send("ada@example.com", "hi");

    EXPECT_THROW(email.send("not-an-address", "hi"), std::invalid_argument);
}

TEST(Part7Email, TransportExceptionsPropagateUnchanged) {
    cc::EmailNotifier email([](std::string_view, std::string_view) {
        throw std::runtime_error("smtp down");
    });
    EXPECT_THROW(email.send("ada@example.com", "hi"), std::runtime_error);
}

TEST(Part7Sms, ValidatesE164AndLength) {
    Email transport;
    EXPECT_CALL(transport, Call(std::string_view{"+15551234567"}, std::string_view{"hi"})).Times(1);
    cc::SMSNotifier sms(transport.AsStdFunction());
    sms.send("+15551234567", "hi");

    EXPECT_THROW(sms.send("15551234567", "hi"), std::invalid_argument);
    EXPECT_THROW(sms.send("+0123456789", "hi"), std::invalid_argument);
    EXPECT_THROW(sms.send("+15551234567", std::string(cc::SMSNotifier::kMaxLength + 1, 'x')),
                 std::invalid_argument);
}

TEST(Part7Webhook, RequiresHttpsEndpoint) {
    EXPECT_THROW(cc::WebhookNotifier("http://hooks.example/a", [](std::string_view, std::string_view) { return 200; }),
                 std::invalid_argument);
}

TEST(Part7Webhook, BodyIsEscapedJsonWithSortedKeys) {
    Post post;
    EXPECT_CALL(post, Call(std::string_view{"https://hooks.example/a"},
                           std::string_view{R"({"message":"say \"hi\"\n","recipient":"ops"})"}))
        .WillOnce(Return(204));
    cc::WebhookNotifier hook("https://hooks.example/a", post.AsStdFunction());
    hook.send("ops", "say \"hi\"\n");
}

TEST(Part7Webhook, NonSuccessStatusThrowsWithStatus) {
    Post post;
    EXPECT_CALL(post, Call(testing::_, testing::_)).WillOnce(Return(500));
    cc::WebhookNotifier hook("https://hooks.example/a", post.AsStdFunction());
    try {
        hook.send("ops", "down?");
        FAIL() << "expected NotificationError";
    } catch (const cc::NotificationError& error) {
        EXPECT_EQ(error.status(), 500);
    }
}

TEST(Part7Dispatcher, ConstructionRejectsNullAndDuplicateChannels) {
    EXPECT_THROW(cc::NotificationDispatcher(notifiers_of(nullptr)), std::invalid_argument);

    auto first = std::make_unique<cc::EmailNotifier>([](std::string_view, std::string_view) {});
    auto second = std::make_unique<cc::EmailNotifier>([](std::string_view, std::string_view) {});
    EXPECT_THROW(cc::NotificationDispatcher(notifiers_of(std::move(first), std::move(second))),
                 std::invalid_argument);
}

TEST(Part7Dispatcher, ChannelsKeepConstructionOrder) {
    cc::NotificationDispatcher dispatcher(notifiers_of(
        std::make_unique<cc::EmailNotifier>([](std::string_view, std::string_view) {}),
        std::make_unique<cc::SMSNotifier>([](std::string_view, std::string_view) {})));
    EXPECT_EQ(dispatcher.size(), 2u);
    EXPECT_EQ(dispatcher.channels(), (std::vector<std::string_view>{"email", "sms"}));
}

TEST(Part7Dispatcher, UnknownChannelThrowsBeforeAnySend) {
    Email email;
    EXPECT_CALL(email, Call(testing::_, testing::_)).Times(0);
    cc::NotificationDispatcher dispatcher(notifiers_of(
        std::make_unique<cc::EmailNotifier>(email.AsStdFunction())));

    EXPECT_THROW((void)dispatcher.dispatch("hi", {{"email", "ada@example.com"}, {"pager", "x"}}),
                 std::out_of_range);
}

TEST(Part7Dispatcher, CapturesPerChannelFailuresAndFollowsRequestOrder) {
    Email email;
    EXPECT_CALL(email, Call(std::string_view{"ada@example.com"}, std::string_view{"hi"})).Times(1);
    cc::NotificationDispatcher dispatcher(notifiers_of(
        std::make_unique<cc::EmailNotifier>(email.AsStdFunction()),
        std::make_unique<cc::SMSNotifier>([](std::string_view, std::string_view) {
            throw std::runtime_error("carrier");
        })));

    auto results = dispatcher.dispatch("hi", {{"sms", "+15551234567"}, {"email", "ada@example.com"}});
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].channel, "sms");
    ASSERT_NE(results[0].error, nullptr);
    EXPECT_THROW(std::rethrow_exception(results[0].error), std::runtime_error);

    EXPECT_EQ(results[1].channel, "email");
    EXPECT_EQ(results[1].error, nullptr);
}
