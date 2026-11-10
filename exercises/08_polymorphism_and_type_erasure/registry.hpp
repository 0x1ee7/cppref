// Part 2 - abstract factories and a name-keyed plugin registry for channels.
#pragma once

#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "interfaces.hpp"

namespace cppref::polymorphism {

/// String key/value configuration, as read from a config file or the environment.
using ChannelConfig = std::map<std::string, std::string, std::less<>>;

/// Abstract factory: each concrete factory knows how to build one channel kind.
/// Configuration problems are reported as an error message, not an exception.
class ChannelFactory {
public:
    virtual ~ChannelFactory() = default;

    /// Registry key, e.g. "email".
    [[nodiscard]] virtual std::string_view name() const = 0;

    [[nodiscard]] virtual std::expected<std::unique_ptr<Channel>, std::string> create(
        const ChannelConfig& config) const = 0;
};

/// Requires the "address" key; it must be non-empty.
class EmailFactory final : public ChannelFactory {
public:
    [[nodiscard]] std::string_view name() const override;
    [[nodiscard]] std::expected<std::unique_ptr<Channel>, std::string> create(
        const ChannelConfig& config) const override;
};

/// Requires the "url" key; it must start with "https://".
class WebhookFactory final : public ChannelFactory {
public:
    [[nodiscard]] std::string_view name() const override;
    [[nodiscard]] std::expected<std::unique_ptr<Channel>, std::string> create(
        const ChannelConfig& config) const override;
};

/// Plugin registry: owns one factory per name.
class ChannelRegistry {
public:
    /// Registers a factory under its name(). Throws std::invalid_argument for a
    /// null factory or a duplicate name ("channel factory 'x' already registered").
    void add(std::unique_ptr<ChannelFactory> factory);

    /// Unknown names return an error ("unknown channel 'x'"). Otherwise the
    /// factory's result is returned unchanged.
    [[nodiscard]] std::expected<std::unique_ptr<Channel>, std::string> create(std::string_view name,
                                                                              const ChannelConfig& config) const;

    /// Registered names, sorted.
    [[nodiscard]] std::vector<std::string> names() const;

    // Engineer: data members (name -> owned factory).
};

/// A registry preloaded with EmailFactory and WebhookFactory.
[[nodiscard]] ChannelRegistry make_default_registry();

}  // namespace cppref::polymorphism
