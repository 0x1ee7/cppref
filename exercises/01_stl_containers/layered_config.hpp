#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace cppref::stl {

struct Value;

/// Keys are sorted; `std::less<>` allows string_view lookups.
/// Note: `Value` is incomplete here. `std::map` with an incomplete mapped type is
/// not formally guaranteed but is what every mainstream implementation supports
/// (nlohmann::json relies on it too).
using Table = std::map<std::string, Value, std::less<>>;
using List = std::vector<Value>;

/// A configuration node. `std::monostate` is null: a key that is present with no value.
struct Value {
    std::variant<std::monostate, bool, std::int64_t, std::string, List, Table> data;
};

/// Which layer supplied a value. The enumerators are NOT in precedence order.
enum class Layer { cli, env, file, defaults };

/// Recursive merge. Where both values are tables, merge key by key. Otherwise the
/// override wins: lists are replaced, and null overrides. Neither input is modified.
/// The result shares no storage with either input.
Value deep_merge(const Value& base, const Value& override_value);

/// Parses `<prefix>_SEGMENT__SEGMENT` variables into a nested table.
///  - Only keys starting with `"<prefix>_"` are considered; the rest is ignored.
///  - The remainder is split on `"__"` and lowercased: `APP_DB__POOL__SIZE` -> db.pool.size.
///  - Values: `true`/`false` (case-insensitive) -> bool; an optional leading `-`
///    followed by ASCII digits -> int64; anything else stays a string.
///  - An empty segment (`APP_`, `APP_DB____HOST`, `APP_DB__`) throws std::invalid_argument.
Table parse_env(const std::map<std::string, std::string>& vars, std::string_view prefix);

/// The four layers handed to `Config`. Defaults to empty tables.
struct ConfigLayers {
    Table defaults;
    Table file;
    Table env;
    Table cli;
};

/// Layered configuration. Precedence, highest first: cli > env > file > defaults.
class Config {
public:
    /// Copies every layer. Later changes to the caller's `layers` have no effect.
    explicit Config(const ConfigLayers& layers);

    /// Dotted lookup ("db.pool.size"). The highest-precedence layer that contains the
    /// *full* path wins. A null value still counts as present. A path that runs
    /// through a non-table is absent in that layer. Returns nullptr if no layer has it.
    /// The pointer stays valid for the lifetime of the Config.
    const Value* get(std::string_view path) const;

    /// The layer that supplied `get(path)`, or `std::nullopt` if absent.
    std::optional<Layer> source(std::string_view path) const;

    /// The fully merged effective configuration, as a fresh table.
    Table as_dict() const;

private:
    // Engineer: store the four layer snapshots (precedence applied on lookup or
    // pre-merged; either works as long as get/source/as_dict agree).
};

}  // namespace cppref::stl
