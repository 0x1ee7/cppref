// Part 7: type-erased core of the event bus. Engineer-owned.
#include "event_bus.hpp"

#include <stdexcept>

namespace cppref::templates_and_concepts {

// STUB: every definition throws until the engineer implements the registry.

EventBus::EventBus() {
  throw std::logic_error("not implemented: EventBus::EventBus");
}

EventBus::EventBus(EventBus&& other) noexcept = default;
EventBus& EventBus::operator=(EventBus&& other) noexcept = default;
EventBus::~EventBus() = default;

Subscription EventBus::subscribe_erased(std::type_index type,
                                        std::function<void(const void*)> handler) {
  (void)type;
  (void)handler;
  throw std::logic_error("not implemented: EventBus::subscribe_erased");
}

std::size_t EventBus::publish_erased(std::type_index type, const void* event) {
  (void)type;
  (void)event;
  throw std::logic_error("not implemented: EventBus::publish_erased");
}

std::size_t EventBus::count_erased(std::type_index type) const {
  (void)type;
  throw std::logic_error("not implemented: EventBus::count_erased");
}

Subscription::Subscription(Subscription&& other) noexcept = default;
Subscription& Subscription::operator=(Subscription&& other) noexcept = default;
Subscription::~Subscription() = default;

void Subscription::cancel() noexcept {
  // Stub: cannot throw from noexcept; the tests fail on active() instead.
}

bool Subscription::active() const noexcept {
  return false;
}

Subscription::Subscription(std::weak_ptr<detail::Registry> registry,
                           std::uint64_t id)
    : registry_(std::move(registry)), id_(id) {}

}  // namespace cppref::templates_and_concepts
