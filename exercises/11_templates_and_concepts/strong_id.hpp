// Part 4: strong typedefs with a phantom tag.
#pragma once

#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>

namespace cppref::templates_and_concepts {

/// A distinct integer id for each `Tag`. Zero-cost: same size and layout as
/// `Rep`, trivially copyable.
///
/// Rules: explicit construction from `Rep` only (no implicit conversion in
/// either direction), value comparison and ordering only between StrongIds of
/// the same Tag, usable as an unordered_map key via std::hash.
///
/// STUB: functions throw (value() returns 0, noexcept). The engineer keeps the
/// single data member `Rep value_` (already declared below).
template <typename Tag, std::unsigned_integral Rep = std::uint64_t>
class StrongId {
 public:
  using rep_type = Rep;

  explicit StrongId(Rep value) : value_(value) {
    throw std::logic_error("not implemented: StrongId::StrongId");
  }

  /// The wrapped integer. STUB: returns 0 (noexcept).
  Rep value() const noexcept { return 0; }

  friend bool operator==(const StrongId& lhs, const StrongId& rhs) {
    (void)lhs;
    (void)rhs;
    throw std::logic_error("not implemented: StrongId::operator==");
  }

  friend std::strong_ordering operator<=>(const StrongId& lhs,
                                          const StrongId& rhs) {
    (void)lhs;
    (void)rhs;
    throw std::logic_error("not implemented: StrongId::operator<=>");
  }

 private:
  Rep value_{};
};

struct UserTag {};
struct OrderTag {};

using UserId = StrongId<UserTag>;
using OrderId = StrongId<OrderTag>;

}  // namespace cppref::templates_and_concepts

namespace std {

/// Hash for StrongId, delegating to the hash of its representation.
/// STUB: throws.
template <typename Tag, typename Rep>
struct hash<cppref::templates_and_concepts::StrongId<Tag, Rep>> {
  std::size_t operator()(
      const cppref::templates_and_concepts::StrongId<Tag, Rep>& id) const {
    (void)id;
    throw std::logic_error("not implemented: hash<StrongId>");
  }
};

}  // namespace std
