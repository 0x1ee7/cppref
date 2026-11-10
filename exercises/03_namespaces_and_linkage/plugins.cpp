#include "plugins.hpp"

#include <stdexcept>

namespace cppref::plugins {

PluginRegistry& PluginRegistry::global() {
    throw std::logic_error("not implemented: PluginRegistry::global");
}

void PluginRegistry::add(std::string, Factory) {
    throw std::logic_error("not implemented: PluginRegistry::add");
}

std::unique_ptr<Plugin> PluginRegistry::create(std::string_view) const {
    throw std::logic_error("not implemented: PluginRegistry::create");
}

std::vector<std::string> PluginRegistry::names() const {
    throw std::logic_error("not implemented: PluginRegistry::names");
}

// Empty on purpose: throwing from a namespace-scope constructor would terminate the
// program before main. The engineer's body must call PluginRegistry::global().add(...).
Registrar::Registrar(std::string, PluginRegistry::Factory) {}

}  // namespace cppref::plugins
