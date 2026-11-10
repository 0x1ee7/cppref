#include "registry.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace cppref::polymorphism {

std::string_view EmailFactory::name() const {
    return "email";
}

std::expected<std::unique_ptr<Channel>, std::string> EmailFactory::create(const ChannelConfig& config) const {
    const auto it = config.find("address");
    if (it == config.end()) {
        return std::unexpected(std::string("missing 'address'"));
    }
    if (it->second.empty()) {
        return std::unexpected(std::string("address must not be empty"));
    }
    return std::make_unique<EmailChannel>(it->second);
}

std::string_view WebhookFactory::name() const {
    return "webhook";
}

std::expected<std::unique_ptr<Channel>, std::string> WebhookFactory::create(const ChannelConfig& config) const {
    const auto it = config.find("url");
    if (it == config.end()) {
        return std::unexpected(std::string("missing 'url'"));
    }
    if (!it->second.starts_with("https://")) {
        return std::unexpected(std::string("url must start with 'https://'"));
    }
    return std::make_unique<WebhookChannel>(it->second);
}

void ChannelRegistry::add(std::unique_ptr<ChannelFactory> factory) {
    if (!factory) {
        throw std::invalid_argument("channel factory must not be null");
    }
    std::string key(factory->name());
    if (factories_.contains(key)) {
        throw std::invalid_argument("channel factory '" + key + "' already registered");
    }
    factories_.emplace(std::move(key), std::move(factory));
}

std::expected<std::unique_ptr<Channel>, std::string> ChannelRegistry::create(std::string_view name,
                                                                             const ChannelConfig& config) const {
    const auto it = factories_.find(name);
    if (it == factories_.end()) {
        return std::unexpected("unknown channel '" + std::string(name) + "'");
    }
    // Virtual dispatch: the concrete factory decides what to build.
    return it->second->create(config);
}

std::vector<std::string> ChannelRegistry::names() const {
    std::vector<std::string> out;
    out.reserve(factories_.size());
    for (const auto& entry : factories_) {
        out.push_back(entry.first);
    }
    return out;
}

ChannelRegistry make_default_registry() {
    ChannelRegistry registry;
    registry.add(std::make_unique<EmailFactory>());
    registry.add(std::make_unique<WebhookFactory>());
    return registry;
}

}  // namespace cppref::polymorphism
