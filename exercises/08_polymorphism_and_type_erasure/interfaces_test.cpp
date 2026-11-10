// Part 1: virtual interfaces, NVI, destructors, slicing, clone.
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "interfaces.hpp"

namespace poly = cppref::polymorphism;

namespace {

class MockChannel final : public poly::Channel {
public:
    MOCK_METHOD(void, hook, (std::string));

    [[nodiscard]] std::unique_ptr<poly::Channel> clone() const override { return nullptr; }

private:
    void do_send(std::string_view message) override { hook(std::string(message)); }
};

class Tracked final : public poly::Channel {
public:
    explicit Tracked(bool& destroyed) : destroyed_(destroyed) {}
    ~Tracked() override { destroyed_ = true; }

    [[nodiscard]] std::unique_ptr<poly::Channel> clone() const override { return nullptr; }

private:
    void do_send(std::string_view /*message*/) override {}
    bool& destroyed_;
};

}  // namespace

TEST(Part1Interfaces, SendForwardsMessageToDoSendOnce) {
    MockChannel ch;
    EXPECT_CALL(ch, hook("hello")).Times(1);
    ch.send("hello");
    EXPECT_EQ(ch.sent_count(), 1U);
}

TEST(Part1Interfaces, SentCountCountsSuccessfulSends) {
    MockChannel ch;
    EXPECT_CALL(ch, hook(::testing::_)).Times(2);
    ch.send("a");
    ch.send("b");
    EXPECT_EQ(ch.sent_count(), 2U);
}

TEST(Part1Interfaces, EmptyMessageIsRejectedBeforeDoSend) {
    MockChannel ch;
    EXPECT_CALL(ch, hook(::testing::_)).Times(0);
    EXPECT_THROW(ch.send(""), std::invalid_argument);
    EXPECT_EQ(ch.sent_count(), 0U);
}

TEST(Part1Interfaces, FailedDoSendIsNotCounted) {
    MockChannel ch;
    EXPECT_CALL(ch, hook(::testing::_)).WillOnce(::testing::Throw(std::runtime_error("down")));
    EXPECT_THROW(ch.send("x"), std::runtime_error);
    EXPECT_EQ(ch.sent_count(), 0U);
}

TEST(Part1Interfaces, EmailChannelOutboxRecordsDeliveriesInOrder) {
    poly::EmailChannel email("ops@example.com");
    email.send("one");
    email.send("two");
    EXPECT_EQ(email.address(), "ops@example.com");
    EXPECT_EQ(email.outbox(), (std::vector<std::string>{"one", "two"}));
    EXPECT_EQ(email.sent_count(), 2U);
}

TEST(Part1Interfaces, WebhookChannelRecordsDeliveries) {
    poly::WebhookChannel hook("https://hooks.example.com/in");
    hook.send("payload");
    EXPECT_EQ(hook.url(), "https://hooks.example.com/in");
    EXPECT_EQ(hook.outbox(), (std::vector<std::string>{"payload"}));
    EXPECT_EQ(hook.sent_count(), 1U);
}

TEST(Part1Interfaces, KindIsDynamicWhileConstructedAsIsBase) {
    poly::EmailChannel email("a@b.c");
    const poly::Channel& base = email;
    EXPECT_EQ(base.kind(), "email");
    EXPECT_EQ(base.constructed_as(), "channel");
}

TEST(Part1Interfaces, DeletingThroughBaseRunsDerivedDestructor) {
    bool destroyed = false;
    {
        std::unique_ptr<poly::Channel> ch = std::make_unique<Tracked>(destroyed);
        EXPECT_FALSE(destroyed);
    }
    EXPECT_TRUE(destroyed);
}

TEST(Part1Interfaces, NoticeCopyIsProtectedAndAlertRendersThroughBase) {
    static_assert(!std::is_copy_constructible_v<poly::Notice>, "Notice copy must be protected");
    std::unique_ptr<poly::Notice> notice = std::make_unique<poly::Alert>("disk full", 2);
    EXPECT_EQ(notice->render(), "alert[2]: disk full");
    EXPECT_EQ(poly::Notice("hi").render(), "notice: hi");
}

TEST(Part1Interfaces, CloneKeepsDynamicType) {
    std::unique_ptr<poly::Notice> original = std::make_unique<poly::Alert>("cpu", 3);
    std::unique_ptr<poly::Notice> copy = original->clone();
    ASSERT_NE(copy, nullptr);
    EXPECT_NE(copy.get(), original.get());
    EXPECT_NE(dynamic_cast<poly::Alert*>(copy.get()), nullptr);
    EXPECT_EQ(copy->render(), "alert[3]: cpu");

    poly::EmailChannel email("ops@example.com");
    std::unique_ptr<poly::Channel> ch = email.clone();
    ASSERT_NE(ch, nullptr);
    EXPECT_EQ(ch->kind(), "email");
}
