// Part 7: event bus keyed by event type, RAII subscriptions.
#include <gtest/gtest.h>

#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "event_bus.hpp"

namespace tc = cppref::templates_and_concepts;

namespace {

struct UserCreated {
  int id;
};

struct OrderPlaced {
  int total;
};

}  // namespace

TEST(Part7EventBus, PublishInvokesMatchingHandlersInSubscriptionOrder) {
  tc::EventBus bus;
  std::vector<std::string> log;
  auto a = bus.subscribe<UserCreated>(
      [&](const UserCreated& e) { log.push_back("a" + std::to_string(e.id)); });
  auto b = bus.subscribe<UserCreated>(
      [&](const UserCreated& e) { log.push_back("b" + std::to_string(e.id)); });
  EXPECT_EQ(bus.publish(UserCreated{1}), 2u);
  EXPECT_EQ(log, (std::vector<std::string>{"a1", "b1"}));
}

TEST(Part7EventBus, HandlersAreKeyedByEventType) {
  tc::EventBus bus;
  int orders = 0;
  auto sub = bus.subscribe<OrderPlaced>([&](const OrderPlaced&) { ++orders; });
  EXPECT_EQ(bus.publish(UserCreated{1}), 0u);
  EXPECT_EQ(bus.publish(OrderPlaced{99}), 1u);
  EXPECT_EQ(orders, 1);
}

TEST(Part7EventBus, CancelStopsDeliveryAndIsIdempotent) {
  tc::EventBus bus;
  int calls = 0;
  auto sub = bus.subscribe<UserCreated>([&](const UserCreated&) { ++calls; });
  EXPECT_EQ(bus.publish(UserCreated{1}), 1u);
  EXPECT_TRUE(sub.active());
  sub.cancel();
  sub.cancel();  // second cancel is a no-op
  EXPECT_FALSE(sub.active());
  EXPECT_EQ(bus.publish(UserCreated{2}), 0u);
  EXPECT_EQ(calls, 1);
}

TEST(Part7EventBus, DestructionCancelsRegistration) {
  tc::EventBus bus;
  int calls = 0;
  {
    auto sub = bus.subscribe<UserCreated>([&](const UserCreated&) { ++calls; });
    EXPECT_EQ(bus.subscriber_count<UserCreated>(), 1u);
  }
  EXPECT_EQ(bus.subscriber_count<UserCreated>(), 0u);
  EXPECT_EQ(bus.publish(UserCreated{1}), 0u);
  EXPECT_EQ(calls, 0);
}

TEST(Part7EventBus, SameCallableSubscribedTwiceIsTwoRegistrations) {
  tc::EventBus bus;
  int calls = 0;
  auto handler = [&](const UserCreated&) { ++calls; };
  auto first = bus.subscribe<UserCreated>(handler);
  auto second = bus.subscribe<UserCreated>(handler);
  EXPECT_EQ(bus.publish(UserCreated{1}), 2u);
  EXPECT_EQ(calls, 2);
  first.cancel();  // removes only its own registration
  EXPECT_EQ(bus.publish(UserCreated{2}), 1u);
  EXPECT_EQ(calls, 3);
}

TEST(Part7EventBus, MoveTransfersTheRegistration) {
  tc::EventBus bus;
  int calls = 0;
  auto a = bus.subscribe<UserCreated>([&](const UserCreated&) { ++calls; });
  tc::Subscription b = std::move(a);
  EXPECT_FALSE(a.active());
  EXPECT_TRUE(b.active());
  EXPECT_EQ(bus.publish(UserCreated{1}), 1u);
  EXPECT_EQ(calls, 1);
}

TEST(Part7EventBus, SubscribeDuringDispatchMissesTheCurrentEvent) {
  tc::EventBus bus;
  int late_calls = 0;
  std::optional<tc::Subscription> late;
  auto outer = bus.subscribe<UserCreated>([&](const UserCreated&) {
    if (!late) {
      late = bus.subscribe<UserCreated>(
          [&](const UserCreated&) { ++late_calls; });
    }
  });
  EXPECT_EQ(bus.publish(UserCreated{1}), 1u);  // snapshot: late not invoked
  EXPECT_EQ(late_calls, 0);
  EXPECT_EQ(bus.publish(UserCreated{2}), 2u);  // now both are live
  EXPECT_EQ(late_calls, 1);
}

TEST(Part7EventBus, CancelDuringDispatchSkipsLaterHandler) {
  tc::EventBus bus;
  int later_calls = 0;
  std::optional<tc::Subscription> later;
  auto first = bus.subscribe<UserCreated>([&](const UserCreated&) {
    if (later) later->cancel();
  });
  later = bus.subscribe<UserCreated>(
      [&](const UserCreated&) { ++later_calls; });
  EXPECT_EQ(bus.publish(UserCreated{1}), 1u);
  EXPECT_EQ(later_calls, 0);
  EXPECT_FALSE(later->active());
}

TEST(Part7EventBus, ThrowingHandlerPropagatesAndBusStaysUsable) {
  tc::EventBus bus;
  int second_calls = 0;
  auto thrower = bus.subscribe<UserCreated>([](const UserCreated&) {
    throw std::runtime_error("boom");
  });
  auto counter = bus.subscribe<UserCreated>(
      [&](const UserCreated&) { ++second_calls; });
  EXPECT_THROW(bus.publish(UserCreated{1}), std::runtime_error);
  EXPECT_EQ(second_calls, 0);  // handler after the thrower is skipped
  EXPECT_EQ(bus.subscriber_count<UserCreated>(), 2u);
  EXPECT_TRUE(thrower.active());
}

TEST(Part7EventBus, SubscriptionOutlivingBusIsSafeToCancel) {
  std::optional<tc::EventBus> bus;
  bus.emplace();
  auto sub = bus->subscribe<UserCreated>([](const UserCreated&) {});
  EXPECT_TRUE(sub.active());
  bus.reset();
  EXPECT_FALSE(sub.active());
  sub.cancel();  // must not crash
}

TEST(Part7EventBus, BusIsMoveOnlyAndHandlersMustAcceptTheEventType) {
  EXPECT_FALSE((std::is_copy_constructible_v<tc::EventBus>));
  auto on_user = [](const UserCreated&) {};
  EXPECT_FALSE((tc::EventHandler<decltype(on_user), OrderPlaced>));
  EXPECT_TRUE((tc::EventHandler<decltype(on_user), UserCreated>));
  tc::EventBus bus;
  auto sub = bus.subscribe<UserCreated>(on_user);
  EXPECT_TRUE(sub.active());
}
