// Part 4: strong typedefs with a phantom tag.
#pragma once

#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace cppref::templates_and_concepts {

/// Distinct integer id per Tag. Same size and layout as Rep; explicit
/// construction only; comparisons only between the same StrongId type.
template <typename Tag, std::unsigned_integral Rep = std::uint64_t>
class StrongId {
 public:
  using rep_type = Rep;

  constexpr explicit StrongId(Rep value) noexcept : value_(value) {}

  constexpr Rep value() const noexcept { return value_; }

  friend constexpr bool operator==(const StrongId&,
                                   const StrongId&) noexcept = default;
  friend constexpr auto operator<=>(const StrongId&,
                                    const StrongId&) noexcept = default;

 private:
  Rep value_{};
};

struct UserTag {};
struct OrderTag {};

using UserId = StrongId<UserTag>;
using OrderId = StrongId<OrderTag>;

}  // namespace cppref::templates_and_concepts

namespace std {

template <typename Tag, typename Rep>
struct hash<cppref::templates_and_concepts::StrongId<Tag, Rep>> {
  std::size_t operator()(
      const cppref::templates_and_concepts::StrongId<Tag, Rep>& id) const
      noexcept {
    return std::hash<Rep>{}(id.value());
  }
};

}  // namespace std
