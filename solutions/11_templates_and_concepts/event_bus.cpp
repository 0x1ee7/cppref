#include "event_bus.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <typeindex>
#include <utility>
#include <vector>

namespace cppref::templates_and_concepts {

namespace detail {

struct Entry {
  std::type_index type;
  std::uint64_t id;
  std::function<void(const void*)> handler;
  bool alive = true;
};

/// Live registrations. cancel() clears `alive` and removes the entry; publish
/// holds its own shared_ptr copies, so removal during dispatch is safe and
/// the flag tells it to skip cancelled entries.
struct Registry {
  std::uint64_t next_id = 1;
  std::vector<std::shared_ptr<Entry>> entries;
};

}  // namespace detail

Subscription::Subscription(std::weak_ptr<detail::Registry> registry,
                           std::uint64_t id)
    : registry_(std::move(registry)), id_(id) {}

Subscription::Subscription(Subscription&& other) noexcept
    : registry_(std::move(other.registry_)),
      id_(std::exchange(other.id_, 0)) {}

Subscription& Subscription::operator=(Subscription&& other) noexcept {
  if (this != &other) {
    cancel();
    registry_ = std::move(other.registry_);
    id_ = std::exchange(other.id_, 0);
  }
  return *this;
}

Subscription::~Subscription() { cancel(); }

void Subscription::cancel() noexcept {
  if (auto reg = registry_.lock(); reg && id_ != 0) {
    const std::uint64_t id = id_;
    std::erase_if(reg->entries, [id](const std::shared_ptr<detail::Entry>& e) {
      if (e->id != id) return false;
      e->alive = false;
      return true;
    });
  }
  registry_.reset();
  id_ = 0;
}

bool Subscription::active() const noexcept {
  auto reg = registry_.lock();
  if (!reg || id_ == 0) return false;
  const std::uint64_t id = id_;
  return std::ranges::any_of(reg->entries,
                             [id](const std::shared_ptr<detail::Entry>& e) {
                               return e->id == id;
                             });
}

EventBus::EventBus() : registry_(std::make_shared<detail::Registry>()) {}

EventBus::~EventBus() = default;

Subscription EventBus::subscribe_erased(
    std::type_index type, std::function<void(const void*)> handler) {
  if (!registry_) {
    registry_ = std::make_shared<detail::Registry>();
  }
  const std::uint64_t id = registry_->next_id++;
  registry_->entries.push_back(std::make_shared<detail::Entry>(
      detail::Entry{type, id, std::move(handler), true}));
  return Subscription(registry_, id);
}

std::size_t EventBus::publish_erased(std::type_index type, const void* event) {
  if (!registry_) return 0;

  std::vector<std::shared_ptr<detail::Entry>> snapshot;
  for (const auto& entry : registry_->entries) {
    if (entry->type == type) snapshot.push_back(entry);
  }

  std::size_t invoked = 0;
  for (const auto& entry : snapshot) {
    if (!entry->alive) continue;
    entry->handler(event);
    ++invoked;
  }
  return invoked;
}

std::size_t EventBus::count_erased(std::type_index type) const {
  if (!registry_) return 0;
  return static_cast<std::size_t>(std::ranges::count_if(
      registry_->entries,
      [type](const std::shared_ptr<detail::Entry>& entry) {
        return entry->type == type;
      }));
}

}  // namespace cppref::templates_and_concepts
