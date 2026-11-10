#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// Part 6b — a process-wide registry and self-registration from static initialisers.
namespace cppref::plugins {

class Plugin {
public:
    virtual ~Plugin() = default;
    virtual std::string run(std::string_view input) = 0;
};

class PluginRegistry {
public:
    using Factory = std::function<std::unique_ptr<Plugin>()>;

    // Process-wide registry. A function-local static, so it is safe to use from static
    // initialisers in any translation unit (no static-init-order fiasco).
    static PluginRegistry& global();

    // Registers `factory` under `name`. Throws std::invalid_argument on a duplicate name.
    void add(std::string name, Factory factory);

    // A new plugin instance for `name`, or nullptr if the name is unknown.
    std::unique_ptr<Plugin> create(std::string_view name) const;

    // All registered names, sorted ascending.
    std::vector<std::string> names() const;

private:
    // Engineer: storage goes here, e.g. std::map<std::string, Factory, std::less<>> entries_;
};

// Self-registration: construct a namespace-scope Registrar to register at program startup.
class Registrar {
public:
    Registrar(std::string name, PluginRegistry::Factory factory);
};

}  // namespace cppref::plugins
