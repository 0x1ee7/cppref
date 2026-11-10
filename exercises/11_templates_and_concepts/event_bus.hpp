// Part 7: an event bus keyed by event type.
#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <typeinfo>
#include <utility>

namespace cppref::templates_and_concepts {

namespace detail {
struct Registry;  // Engineer: defined in event_bus.cpp.
}  // namespace detail

/// A handler H can be invoked with `const E&`.
template <typename H, typename E>
concept EventHandler = std::invocable<H, const E&>;

/// RAII registration handle. Destruction cancels the registration. Move-only.
/// Cancelling is idempotent; a handle outliving its bus is safe to cancel.
class Subscription {
 public:
  /// Empty handle; `active()` is false.
  Subscription() noexcept = default;
  Subscription(Subscription&& other) noexcept;
  Subscription& operator=(Subscription&& other) noexcept;
  Subscription(const Subscription&) = delete;
  Subscription& operator=(const Subscription&) = delete;
  ~Subscription();

  /// Removes this registration only. Idempotent. Defined in event_bus.cpp.
  void cancel() noexcept;

  /// True while the registration is live. Defined in event_bus.cpp.
  bool active() const noexcept;

 private:
  friend class EventBus;
  Subscription(std::weak_ptr<detail::Registry> registry, std::uint64_t id);

  // Engineer: registry link and registration id live here (declared below).
  std::weak_ptr<detail::Registry> registry_;
  std::uint64_t id_ = 0;
};

/// Synchronous, single-threaded publish/subscribe keyed by `typeid(E)`.
///
/// Dispatch uses a snapshot taken at the start of publish():
///   - handlers subscribed during dispatch are not invoked for that event;
///   - handlers cancelled during dispatch are skipped;
///   - a handler that throws propagates the exception; handlers after it are
///     not invoked for that event; the bus stays usable.
/// Handlers run in subscription order. The same callable subscribed twice is
/// two registrations.
class EventBus {
 public:
  EventBus();
  EventBus(EventBus&& other) noexcept;
  EventBus& operator=(EventBus&& other) noexcept;
  EventBus(const EventBus&) = delete;
  EventBus& operator=(const EventBus&) = delete;
  ~EventBus();

  /// Registers `handler` for events of type E. The result must be kept: the
  /// registration ends when the Subscription is destroyed or cancelled.
  template <typename E, typename H>
    requires EventHandler<H, E>
  [[nodiscard]] Subscription subscribe(H&& handler) {
    (void)handler;
    throw std::logic_error("not implemented: EventBus::subscribe");
  }

  /// Invokes the live handlers for E and returns how many were invoked.
  template <typename E>
  std::size_t publish(const E& event) {
    (void)event;
    throw std::logic_error("not implemented: EventBus::publish");
  }

  /// Number of live registrations for E.
  template <typename E>
  std::size_t subscriber_count() const {
    throw std::logic_error("not implemented: EventBus::subscriber_count");
  }

 private:
  // Type-erased core, implemented in event_bus.cpp.
  Subscription subscribe_erased(std::type_index type,
                                std::function<void(const void*)> handler);
  std::size_t publish_erased(std::type_index type, const void* event);
  std::size_t count_erased(std::type_index type) const;

  // Engineer: shared registry (pimpl) lives here (declared below).
  std::shared_ptr<detail::Registry> registry_;
};

}  // namespace cppref::templates_and_concepts
