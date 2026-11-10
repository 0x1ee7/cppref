#include "registry.hpp"

#include <stdexcept>
#include <string>

namespace cppref::polymorphism {
namespace {
[[noreturn]] void not_implemented(const char* what) {
    throw std::logic_error(std::string("not implemented: ") + what);
}
}  // namespace

std::string_view EmailFactory::name() const { not_implemented("EmailFactory::name"); }

std::expected<std::unique_ptr<Channel>, std::string> EmailFactory::create(const ChannelConfig& /*config*/) const {
    not_implemented("EmailFactory::create");
}

std::string_view WebhookFactory::name() const { not_implemented("WebhookFactory::name"); }

std::expected<std::unique_ptr<Channel>, std::string> WebhookFactory::create(const ChannelConfig& /*config*/) const {
    not_implemented("WebhookFactory::create");
}

void ChannelRegistry::add(std::unique_ptr<ChannelFactory> /*factory*/) {
    not_implemented("ChannelRegistry::add");
}

std::expected<std::unique_ptr<Channel>, std::string> ChannelRegistry::create(std::string_view /*name*/,
                                                                             const ChannelConfig& /*config*/) const {
    not_implemented("ChannelRegistry::create");
}

std::vector<std::string> ChannelRegistry::names() const { not_implemented("ChannelRegistry::names"); }

ChannelRegistry make_default_registry() { not_implemented("make_default_registry"); }

}  // namespace cppref::polymorphism
