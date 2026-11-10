#include "notifications.hpp"

#include <stdexcept>
#include <string>

namespace cppref::classes {
namespace {

[[noreturn]] void not_implemented(const char* name) {
    throw std::logic_error(std::string("not implemented: ") + name);
}

}  // namespace

NotificationError::NotificationError(const std::string&, int) : std::runtime_error("") {
    not_implemented("NotificationError::NotificationError");
}

int NotificationError::status() const noexcept {
    not_implemented("NotificationError::status");
}

std::string Notifier::describe() const {
    not_implemented("Notifier::describe");
}

EmailNotifier::EmailNotifier(Transport) {
    not_implemented("EmailNotifier::EmailNotifier");
}

std::string_view EmailNotifier::channel_name() const {
    not_implemented("EmailNotifier::channel_name");
}

void EmailNotifier::send(std::string_view, std::string_view) {
    not_implemented("EmailNotifier::send");
}

SMSNotifier::SMSNotifier(Transport) {
    not_implemented("SMSNotifier::SMSNotifier");
}

std::string_view SMSNotifier::channel_name() const {
    not_implemented("SMSNotifier::channel_name");
}

void SMSNotifier::send(std::string_view, std::string_view) {
    not_implemented("SMSNotifier::send");
}

WebhookNotifier::WebhookNotifier(std::string, Post) {
    not_implemented("WebhookNotifier::WebhookNotifier");
}

std::string_view WebhookNotifier::channel_name() const {
    not_implemented("WebhookNotifier::channel_name");
}

void WebhookNotifier::send(std::string_view, std::string_view) {
    not_implemented("WebhookNotifier::send");
}

NotificationDispatcher::NotificationDispatcher(std::vector<std::unique_ptr<Notifier>>) {
    not_implemented("NotificationDispatcher::NotificationDispatcher");
}

std::vector<std::string_view> NotificationDispatcher::channels() const {
    not_implemented("NotificationDispatcher::channels");
}

std::size_t NotificationDispatcher::size() const {
    not_implemented("NotificationDispatcher::size");
}

std::vector<DispatchResult> NotificationDispatcher::dispatch(std::string_view, const Recipients&) {
    not_implemented("NotificationDispatcher::dispatch");
}

}  // namespace cppref::classes
