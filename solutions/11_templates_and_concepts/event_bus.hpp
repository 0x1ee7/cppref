// Part 7: an event bus keyed by event type.
#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <utility>

namespace cppref::templates_and_concepts {

namespace detail {
struct Registry;
}  // namespace detail

/// A handler H can be invoked with `const E&`.
template <typename H, typename E>
concept EventHandler = std::invocable<H, const E&>;

/// RAII registration. Destruction or cancel() removes the registration; both
/// are idempotent. Holds a weak link, so it is safe after the bus is gone.
class Subscription {
 public:
  Subscription() noexcept = default;
  Subscription(Subscription&& other) noexcept;
  Subscription& operator=(Subscription&& other) noexcept;
  Subscription(const Subscription&) = delete;
  Subscription& operator=(const Subscription&) = delete;
  ~Subscription();

  void cancel() noexcept;
  bool active() const noexcept;

 private:
  friend class EventBus;
  Subscription(std::weak_ptr<detail::Registry> registry, std::uint64_t id);

  std::weak_ptr<detail::Registry> registry_;
  std::uint64_t id_ = 0;
};

/// Synchronous publish/subscribe keyed by event type. Publish snapshots the
/// live registrations first: handlers added during dispatch wait for the next
/// event; cancelled ones are skipped; a throwing handler propagates and stops
/// dispatch for that event. A moved-from bus is empty and still usable.
class EventBus {
 public:
  EventBus();
  EventBus(EventBus&& other) noexcept = default;
  EventBus& operator=(EventBus&& other) noexcept = default;
  EventBus(const EventBus&) = delete;
  EventBus& operator=(const EventBus&) = delete;
  ~EventBus();

  /// The handler is copied into type-erased storage, so it must be
  /// copy-constructible.
  template <typename E, typename H>
    requires EventHandler<H, E>
  [[nodiscard]] Subscription subscribe(H&& handler) {
    using Stored = std::decay_t<H>;
    return subscribe_erased(
        std::type_index(typeid(E)),
        [h = Stored(std::forward<H>(handler))](const void* event) mutable {
          h(*static_cast<const E*>(event));
        });
  }

  template <typename E>
  std::size_t publish(const E& event) {
    return publish_erased(std::type_index(typeid(E)), std::addressof(event));
  }

  template <typename E>
  std::size_t subscriber_count() const {
    return count_erased(std::type_index(typeid(E)));
  }

 private:
  Subscription subscribe_erased(std::type_index type,
                                std::function<void(const void*)> handler);
  std::size_t publish_erased(std::type_index type, const void* event);
  std::size_t count_erased(std::type_index type) const;

  std::shared_ptr<detail::Registry> registry_;
};

}  // namespace cppref::templates_and_concepts
