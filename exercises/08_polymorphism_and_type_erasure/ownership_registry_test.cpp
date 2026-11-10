// Part 2: polymorphic ownership (Owned<T>) and an abstract-factory plugin registry.
#include <gtest/gtest.h>

#include <expected>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "ownership.hpp"
#include "registry.hpp"

namespace poly = cppref::polymorphism;

namespace {

// A third-party plugin: builds a WebhookChannel from config key "to".
class SinkFactory final : public poly::ChannelFactory {
public:
    [[nodiscard]] std::string_view name() const override { return "sink"; }

    [[nodiscard]] std::expected<std::unique_ptr<poly::Channel>, std::string> create(
        const poly::ChannelConfig& config) const override {
        const auto it = config.find("to");
        if (it == config.end()) {
            return std::unexpected(std::string("sink needs 'to'"));
        }
        return std::make_unique<poly::WebhookChannel>(it->second);
    }
};

}  // namespace

TEST(Part2Ownership, CopyIsDeepAndPreservesDynamicType) {
    poly::Owned<poly::Channel> a(std::make_unique<poly::EmailChannel>("ops@example.com"));
    poly::Owned<poly::Channel> b = a;
    EXPECT_NE(a.get(), b.get());
    ASSERT_TRUE(static_cast<bool>(b));
    EXPECT_EQ(b->kind(), "email");
    b->send("hello");
    EXPECT_EQ(a->sent_count(), 0U);
    EXPECT_EQ(b->sent_count(), 1U);
}

TEST(Part2Ownership, MoveLeavesSourceEmpty) {
    poly::Owned<poly::Channel> a(std::make_unique<poly::WebhookChannel>("https://hooks.example.com/a"));
    poly::Owned<poly::Channel> b = std::move(a);
    EXPECT_FALSE(static_cast<bool>(a));
    EXPECT_EQ(a.get(), nullptr);
    ASSERT_TRUE(static_cast<bool>(b));
    EXPECT_EQ(b->kind(), "webhook");
}

TEST(Part2Ownership, EmptyCopyStaysEmptyAndSelfAssignIsSafe) {
    poly::Owned<poly::Channel> empty(std::unique_ptr<poly::Channel>{});
    poly::Owned<poly::Channel> copy = empty;
    EXPECT_FALSE(static_cast<bool>(copy));

    poly::Owned<poly::Channel> a(std::make_unique<poly::EmailChannel>("x@y.z"));
    poly::Owned<poly::Channel>& alias = a;
    a = alias;
    ASSERT_TRUE(static_cast<bool>(a));
    EXPECT_EQ(a->kind(), "email");
}

TEST(Part2Ownership, AssignmentReplacesTargetPolymorphically) {
    poly::Owned<poly::Channel> a(std::make_unique<poly::EmailChannel>("x@y.z"));
    poly::Owned<poly::Channel> b(std::make_unique<poly::WebhookChannel>("https://h.example/"));
    a = b;
    ASSERT_TRUE(static_cast<bool>(a));
    ASSERT_TRUE(static_cast<bool>(b));
    EXPECT_NE(a.get(), b.get());
    EXPECT_EQ(a->kind(), "webhook");
    EXPECT_EQ(b->kind(), "webhook");

    b = std::move(a);
    EXPECT_FALSE(static_cast<bool>(a));
    EXPECT_EQ(&*b, b.get());
    EXPECT_EQ(b->kind(), "webhook");
}

TEST(Part2Registry, DefaultRegistryListsBuiltInNames) {
    const auto reg = poly::make_default_registry();
    EXPECT_EQ(reg.names(), (std::vector<std::string>{"email", "webhook"}));
}

TEST(Part2Registry, CreatesEmailChannelFromConfig) {
    const auto reg = poly::make_default_registry();
    const poly::ChannelConfig cfg{{"address", "ops@example.com"}};
    auto result = reg.create("email", cfg);
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ((*result)->kind(), "email");
    auto* email = dynamic_cast<poly::EmailChannel*>(result->get());
    ASSERT_NE(email, nullptr);
    EXPECT_EQ(email->address(), "ops@example.com");
}

TEST(Part2Registry, EmailMissingAddressIsError) {
    const auto reg = poly::make_default_registry();
    auto result = reg.create("email", {});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "missing 'address'");
}

TEST(Part2Registry, EmailEmptyAddressIsError) {
    const auto reg = poly::make_default_registry();
    const poly::ChannelConfig cfg{{"address", ""}};
    auto result = reg.create("email", cfg);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "address must not be empty");
}

TEST(Part2Registry, WebhookValidatesUrl) {
    const auto reg = poly::make_default_registry();

    auto missing = reg.create("webhook", {});
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error(), "missing 'url'");

    const poly::ChannelConfig insecure{{"url", "http://hooks.example.com/"}};
    auto bad = reg.create("webhook", insecure);
    ASSERT_FALSE(bad.has_value());
    EXPECT_EQ(bad.error(), "url must start with 'https://'");

    const poly::ChannelConfig good{{"url", "https://hooks.example.com/"}};
    auto ok = reg.create("webhook", good);
    ASSERT_TRUE(ok.has_value()) << ok.error();
    EXPECT_EQ((*ok)->kind(), "webhook");
}

TEST(Part2Registry, UnknownNameIsError) {
    const auto reg = poly::make_default_registry();
    auto result = reg.create("sms", {});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "unknown channel 'sms'");
}

TEST(Part2Registry, DuplicateAndNullRegistrationThrow) {
    auto reg = poly::make_default_registry();
    try {
        reg.add(std::make_unique<poly::EmailFactory>());
        ADD_FAILURE() << "expected std::invalid_argument for duplicate name";
    } catch (const std::invalid_argument& e) {
        EXPECT_EQ(std::string(e.what()), "channel factory 'email' already registered");
    }
    EXPECT_THROW(reg.add(nullptr), std::invalid_argument);
}

TEST(Part2Registry, CustomFactoryPlugsIntoRegistry) {
    auto reg = poly::make_default_registry();
    reg.add(std::make_unique<SinkFactory>());
    EXPECT_EQ(reg.names(), (std::vector<std::string>{"email", "sink", "webhook"}));

    const poly::ChannelConfig cfg{{"to", "https://sink.example/in"}};
    auto result = reg.create("sink", cfg);
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ((*result)->kind(), "webhook");

    auto missing = reg.create("sink", {});
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error(), "sink needs 'to'");
}
