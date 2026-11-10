// Part 2: a constrained generic class template.
#pragma once

#include <concepts>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <vector>

#include "concepts.hpp"

namespace cppref::templates_and_concepts {

/// In-memory repository keyed by `T::id_type`.
///
/// Requires `Entity<T>`. Keeps at most one entry per id. Iteration order is
/// ascending by id (use an ordered container internally; the engineer picks
/// which one and why).
///
/// STUB: members throw (and hold no state yet). The engineer adds the data
/// member here (e.g. std::map<id_type, T>).
template <typename T>
  requires Entity<T>
class Repository {
 public:
  using id_type = typename T::id_type;

  /// Inserts `item` under `item.id()`. Returns false, leaving the stored
  /// value untouched, if the id is already present.
  bool add(T item) {
    (void)item;
    throw std::logic_error("not implemented: Repository::add");
  }

  /// Copy of the stored entity, or an empty optional.
  std::optional<T> find(const id_type& id) const {
    (void)id;
    throw std::logic_error("not implemented: Repository::find");
  }

  /// Removes the entity with `id`. Returns false if it was absent.
  bool remove(const id_type& id) {
    (void)id;
    throw std::logic_error("not implemented: Repository::remove");
  }

  /// Number of stored entities. STUB: returns 0 (noexcept).
  std::size_t size() const noexcept { return 0; }

  /// True when the repository holds no entities. STUB: returns true (noexcept).
  bool empty() const noexcept { return true; }

  /// Snapshot of all entities in ascending id order.
  std::vector<T> all() const {
    throw std::logic_error("not implemented: Repository::all");
  }

  /// Snapshot of the entities for which `pred(entity)` is true, in id order.
  template <std::predicate<const T&> P>
  std::vector<T> where(P&& pred) const {
    (void)pred;
    throw std::logic_error("not implemented: Repository::where");
  }
};

}  // namespace cppref::templates_and_concepts
