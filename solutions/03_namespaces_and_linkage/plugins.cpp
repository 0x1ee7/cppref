#include "plugins.hpp"

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cppref::plugins {

PluginRegistry& PluginRegistry::global() {
    static PluginRegistry registry;
    return registry;
}

void PluginRegistry::add(std::string name, Factory factory) {
    if (entries_.contains(name)) {
        throw std::invalid_argument("duplicate plugin: " + name);
    }
    entries_.emplace(std::move(name), std::move(factory));
}

std::unique_ptr<Plugin> PluginRegistry::create(std::string_view name) const {
    const auto it = entries_.find(name);
    if (it == entries_.end()) return nullptr;
    return it->second();
}

std::vector<std::string> PluginRegistry::names() const {
    std::vector<std::string> out;
    out.reserve(entries_.size());
    for (const auto& entry : entries_) out.push_back(entry.first);
    return out;
}

Registrar::Registrar(std::string name, PluginRegistry::Factory factory) {
    PluginRegistry::global().add(std::move(name), std::move(factory));
}

}  // namespace cppref::plugins
