// Part 2: a constrained generic class template.
#pragma once

#include <concepts>
#include <cstddef>
#include <map>
#include <optional>
#include <vector>

#include "concepts.hpp"

namespace cppref::templates_and_concepts {

/// In-memory repository keyed by `T::id_type`. Ordered map gives ascending id
/// iteration. Copies in and out, so the repository owns its entities.
template <typename T>
  requires Entity<T>
class Repository {
 public:
  using id_type = typename T::id_type;

  /// Inserts `item` under its id. Returns false (and keeps the original) if
  /// the id already exists.
  bool add(T item) {
    id_type id = item.id();
    return items_.try_emplace(std::move(id), std::move(item)).second;
  }

  std::optional<T> find(const id_type& id) const {
    if (auto it = items_.find(id); it != items_.end()) {
      return it->second;
    }
    return std::nullopt;
  }

  bool remove(const id_type& id) { return items_.erase(id) > 0; }

  std::size_t size() const noexcept { return items_.size(); }

  bool empty() const noexcept { return items_.empty(); }

  std::vector<T> all() const {
    std::vector<T> out;
    out.reserve(items_.size());
    for (const auto& entry : items_) {
      out.push_back(entry.second);
    }
    return out;
  }

  template <std::predicate<const T&> P>
  std::vector<T> where(P&& pred) const {
    std::vector<T> out;
    for (const auto& entry : items_) {
      if (pred(entry.second)) {
        out.push_back(entry.second);
      }
    }
    return out;
  }

 private:
  std::map<id_type, T> items_;
};

}  // namespace cppref::templates_and_concepts
