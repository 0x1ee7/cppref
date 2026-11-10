#include "interfaces.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace cppref::polymorphism {
namespace {
[[noreturn]] void not_implemented(const char* what) {
    throw std::logic_error(std::string("not implemented: ") + what);
}
}  // namespace

Channel::Channel() { not_implemented("Channel::Channel"); }

void Channel::send(std::string_view /*message*/) { not_implemented("Channel::send"); }

std::size_t Channel::sent_count() const { not_implemented("Channel::sent_count"); }

std::string_view Channel::kind() const { not_implemented("Channel::kind"); }

std::string_view Channel::constructed_as() const { not_implemented("Channel::constructed_as"); }

EmailChannel::EmailChannel(std::string /*address*/) { not_implemented("EmailChannel::EmailChannel"); }

std::string_view EmailChannel::kind() const { not_implemented("EmailChannel::kind"); }

std::unique_ptr<Channel> EmailChannel::clone() const { not_implemented("EmailChannel::clone"); }

const std::string& EmailChannel::address() const { not_implemented("EmailChannel::address"); }

const std::vector<std::string>& EmailChannel::outbox() const { not_implemented("EmailChannel::outbox"); }

void EmailChannel::do_send(std::string_view /*message*/) { not_implemented("EmailChannel::do_send"); }

WebhookChannel::WebhookChannel(std::string /*url*/) { not_implemented("WebhookChannel::WebhookChannel"); }

std::string_view WebhookChannel::kind() const { not_implemented("WebhookChannel::kind"); }

std::unique_ptr<Channel> WebhookChannel::clone() const { not_implemented("WebhookChannel::clone"); }

const std::string& WebhookChannel::url() const { not_implemented("WebhookChannel::url"); }

const std::vector<std::string>& WebhookChannel::outbox() const { not_implemented("WebhookChannel::outbox"); }

void WebhookChannel::do_send(std::string_view /*message*/) { not_implemented("WebhookChannel::do_send"); }

Notice::Notice(std::string /*text*/) { not_implemented("Notice::Notice"); }

std::string Notice::render() const { not_implemented("Notice::render"); }

std::unique_ptr<Notice> Notice::clone() const { not_implemented("Notice::clone"); }

const std::string& Notice::text() const { not_implemented("Notice::text"); }

Alert::Alert(std::string text, int /*severity*/) : Notice(std::move(text)) { not_implemented("Alert::Alert"); }

std::string Alert::render() const { not_implemented("Alert::render"); }

std::unique_ptr<Notice> Alert::clone() const { not_implemented("Alert::clone"); }

int Alert::severity() const { not_implemented("Alert::severity"); }

}  // namespace cppref::polymorphism
