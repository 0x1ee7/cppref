#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cppref::stl {

/// One sample from the ingest socket. `value` is NaN when the sensor read failed;
/// `ts_ms` is negative when the device clock was never synchronised.
struct Reading {
    std::string sensor;
    double value;
    std::int64_t ts_ms;

    friend bool operator==(const Reading&, const Reading&) = default;
};

/// Removes, in place, every reading whose value is NaN or whose `ts_ms` is negative.
/// Survivors keep their relative order. Returns the number removed.
/// Single pass, no reallocation. Use the erase-remove idiom or `std::erase_if`.
std::size_t drop_invalid(std::vector<Reading>& readings);

/// Copies the readings from `sensor`, in input order. Capacity is reserved to the
/// exact match count so the result never reallocates while it is being filled.
std::vector<Reading> select_sensor(std::span<const Reading> readings, std::string_view sensor);

/// Splits `line` on `sep`. Empty fields are kept: "a,,b" -> {"a", "", "b"} and
/// "" -> {""}. The returned views alias `line`; they dangle once the storage
/// behind `line` is gone.
std::vector<std::string_view> split_fields(std::string_view line, char sep);

/// The sensor name of the non-NaN reading with the highest value. Ties go to the
/// earlier reading. The view aliases `readings[i].sensor` (no copy). Empty or
/// all-NaN input yields `std::nullopt`.
std::optional<std::string_view> hottest_sensor(std::span<const Reading> readings);

}  // namespace cppref::stl
