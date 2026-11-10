#include "layered_config.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace cppref::stl {
namespace {

// Dotted-path lookup inside one layer. Returns nullptr if the path is absent.
const Value* lookup(const Table& root, std::string_view path) {
    const Table* table = &root;
    while (true) {
        const std::size_t dot = path.find('.');
        const auto it = table->find(path.substr(0, dot));
        if (it == table->end()) return nullptr;
        if (dot == std::string_view::npos) return &it->second;
        table = std::get_if<Table>(&it->second.data);
        if (table == nullptr) return nullptr;
        path.remove_prefix(dot + 1);
    }
}

std::string to_lower(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

// bool, then int64 (from_chars rejects '+', overflow, and trailing junk), else string.
Value coerce(std::string_view raw) {
    const std::string lowered = to_lower(raw);
    if (lowered == "true") return Value{true};
    if (lowered == "false") return Value{false};

    std::int64_t number = 0;
    const char* const end = raw.data() + raw.size();
    const auto [ptr, ec] = std::from_chars(raw.data(), end, number);
    if (ec == std::errc{} && ptr == end) return Value{number};
    return Value{std::string(raw)};
}

}  // namespace

Value deep_merge(const Value& base, const Value& override_value) {
    const auto* base_table = std::get_if<Table>(&base.data);
    const auto* over_table = std::get_if<Table>(&override_value.data);
    if (base_table == nullptr || over_table == nullptr) return override_value;

    Table merged = *base_table;
    for (const auto& [key, over] : *over_table) {
        if (auto it = merged.find(key); it != merged.end()) {
            it->second = deep_merge(it->second, over);
        } else {
            merged.emplace(key, over);
        }
    }
    return Value{std::move(merged)};
}

Table parse_env(const std::map<std::string, std::string>& vars, std::string_view prefix) {
    const std::string head = std::string(prefix) + "_";
    Table root;

    for (const auto& [key, raw] : vars) {
        if (key.compare(0, head.size(), head) != 0) continue;

        std::vector<std::string> segments;
        std::string_view rest = std::string_view(key).substr(head.size());
        while (true) {
            const std::size_t sep = rest.find("__");
            const std::string_view seg = rest.substr(0, sep);
            if (seg.empty()) {
                throw std::invalid_argument("parse_env: empty segment in '" + key + "'");
            }
            segments.push_back(to_lower(seg));
            if (sep == std::string_view::npos) break;
            rest.remove_prefix(sep + 2);
        }

        Table* node = &root;
        for (std::size_t i = 0; i + 1 < segments.size(); ++i) {
            Value& slot = (*node)[segments[i]];
            if (!std::holds_alternative<Table>(slot.data)) slot = Value{Table{}};
            node = &std::get<Table>(slot.data);
        }
        (*node)[segments.back()] = coerce(raw);
    }
    return root;
}

Config::Config(const ConfigLayers& layers)
    : layers_{{Layer::cli, layers.cli},
              {Layer::env, layers.env},
              {Layer::file, layers.file},
              {Layer::defaults, layers.defaults}} {}

const Value* Config::get(std::string_view path) const {
    for (const auto& entry : layers_) {
        if (const Value* found = lookup(entry.second, path)) return found;
    }
    return nullptr;
}

std::optional<Layer> Config::source(std::string_view path) const {
    for (const auto& entry : layers_) {
        if (lookup(entry.second, path) != nullptr) return entry.first;
    }
    return std::nullopt;
}

Table Config::as_dict() const {
    // Fold from lowest precedence (defaults) up to highest (cli).
    Value merged{Table{}};
    for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) {
        merged = deep_merge(merged, Value{it->second});
    }
    return std::get<Table>(std::move(merged.data));
}

}  // namespace cppref::stl
