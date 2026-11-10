#include "interfaces.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace cppref::polymorphism {

// ---- Channel ---------------------------------------------------------------

Channel::Channel() {
    // Virtual dispatch here reaches Channel::kind(): the derived part does not exist yet.
    constructed_as_ = kind();
}

void Channel::send(std::string_view message) {
    if (message.empty()) {
        throw std::invalid_argument("message must not be empty");
    }
    do_send(message);
    ++sent_count_;
}

std::size_t Channel::sent_count() const {
    return sent_count_;
}

std::string_view Channel::kind() const {
    return "channel";
}

std::string_view Channel::constructed_as() const {
    return constructed_as_;
}

// ---- EmailChannel ----------------------------------------------------------

EmailChannel::EmailChannel(std::string address) : address_(std::move(address)) {}

std::string_view EmailChannel::kind() const {
    return "email";
}

std::unique_ptr<Channel> EmailChannel::clone() const {
    return std::make_unique<EmailChannel>(*this);
}

const std::string& EmailChannel::address() const {
    return address_;
}

const std::vector<std::string>& EmailChannel::outbox() const {
    return outbox_;
}

void EmailChannel::do_send(std::string_view message) {
    outbox_.emplace_back(message);
}

// ---- WebhookChannel --------------------------------------------------------

WebhookChannel::WebhookChannel(std::string url) : url_(std::move(url)) {}

std::string_view WebhookChannel::kind() const {
    return "webhook";
}

std::unique_ptr<Channel> WebhookChannel::clone() const {
    return std::make_unique<WebhookChannel>(*this);
}

const std::string& WebhookChannel::url() const {
    return url_;
}

const std::vector<std::string>& WebhookChannel::outbox() const {
    return outbox_;
}

void WebhookChannel::do_send(std::string_view message) {
    outbox_.emplace_back(message);
}

// ---- Notice / Alert --------------------------------------------------------

Notice::Notice(std::string text) : text_(std::move(text)) {}

std::string Notice::render() const {
    return "notice: " + text_;
}

std::unique_ptr<Notice> Notice::clone() const {
    // Inside a member, the protected copy constructor is accessible.
    return std::unique_ptr<Notice>(new Notice(*this));
}

const std::string& Notice::text() const {
    return text_;
}

Alert::Alert(std::string text, int severity) : Notice(std::move(text)), severity_(severity) {}

std::string Alert::render() const {
    return "alert[" + std::to_string(severity_) + "]: " + text();
}

std::unique_ptr<Notice> Alert::clone() const {
    return std::make_unique<Alert>(*this);
}

int Alert::severity() const {
    return severity_;
}

}  // namespace cppref::polymorphism
