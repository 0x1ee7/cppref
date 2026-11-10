#include "notifications.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

namespace cppref::classes {
namespace {

bool has_space(std::string_view text) {
    return std::ranges::any_of(text, [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; });
}

/// local@domain with exactly one '@', non-empty parts, no whitespace.
bool valid_address(std::string_view address) {
    const auto at = address.find('@');
    return !has_space(address) && at != std::string_view::npos && at > 0 && at + 1 < address.size() &&
           address.find('@', at + 1) == std::string_view::npos;
}

/// '+', a non-zero first digit, then 7-15 digits in total.
bool valid_e164(std::string_view number) {
    if (number.empty() || number.front() != '+') {
        return false;
    }
    const auto digits = number.substr(1);
    const bool all_digits = std::ranges::all_of(digits, [](char c) { return c >= '0' && c <= '9'; });
    return all_digits && digits.size() >= 7 && digits.size() <= 15 && digits.front() != '0';
}

/// Count UTF-8 code points (continuation bytes are not counted).
std::size_t code_points(std::string_view text) {
    return static_cast<std::size_t>(std::ranges::count_if(text, [](char c) {
        return (static_cast<unsigned char>(c) & 0xC0) != 0x80;
    }));
}

void append_json_string(std::string& out, std::string_view text) {
    constexpr char kHex[] = "0123456789abcdef";
    out.push_back('"');
    for (const char raw : text) {
        const auto c = static_cast<unsigned char>(raw);
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    out += "\\u00";
                    out.push_back(kHex[c >> 4]);
                    out.push_back(kHex[c & 0xF]);
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    out.push_back('"');
}

}  // namespace

// ---------------------------------------------------------------- Notifier

std::string Notifier::describe() const {
    return std::string(channel_name()) + " notifier";
}

// ---------------------------------------------------------------- EmailNotifier

EmailNotifier::EmailNotifier(Transport transport) : transport_(std::move(transport)) {}

std::string_view EmailNotifier::channel_name() const {
    return "email";
}

void EmailNotifier::send(std::string_view recipient, std::string_view message) {
    if (!valid_address(recipient)) {
        throw std::invalid_argument("invalid email address");
    }
    transport_(recipient, message);
}

// ---------------------------------------------------------------- SMSNotifier

SMSNotifier::SMSNotifier(Transport transport) : transport_(std::move(transport)) {}

std::string_view SMSNotifier::channel_name() const {
    return "sms";
}

void SMSNotifier::send(std::string_view recipient, std::string_view message) {
    if (!valid_e164(recipient)) {
        throw std::invalid_argument("recipient is not an E.164 number");
    }
    if (code_points(message) > kMaxLength) {
        throw std::invalid_argument("SMS message exceeds 160 characters");
    }
    transport_(recipient, message);
}

// ---------------------------------------------------------------- WebhookNotifier

WebhookNotifier::WebhookNotifier(std::string endpoint, Post post)
    : endpoint_(std::move(endpoint)), post_(std::move(post)) {
    if (!endpoint_.starts_with("https://")) {
        throw std::invalid_argument("webhook endpoint must use https://");
    }
}

std::string_view WebhookNotifier::channel_name() const {
    return "webhook";
}

void WebhookNotifier::send(std::string_view recipient, std::string_view message) {
    std::string body = R"({"message":)";
    append_json_string(body, message);
    body += R"(,"recipient":)";
    append_json_string(body, recipient);
    body.push_back('}');

    const int status = post_(endpoint_, body);
    if (status < 200 || status > 299) {
        throw NotificationError("webhook returned HTTP " + std::to_string(status), status);
    }
}

// ---------------------------------------------------------------- NotificationDispatcher

NotificationDispatcher::NotificationDispatcher(std::vector<std::unique_ptr<Notifier>> notifiers)
    : notifiers_(std::move(notifiers)) {
    for (std::size_t i = 0; i < notifiers_.size(); ++i) {
        if (!notifiers_[i]) {
            throw std::invalid_argument("notifier must not be null");
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (notifiers_[j]->channel_name() == notifiers_[i]->channel_name()) {
                throw std::invalid_argument("duplicate channel: " + std::string(notifiers_[i]->channel_name()));
            }
        }
    }
}

std::vector<std::string_view> NotificationDispatcher::channels() const {
    std::vector<std::string_view> names;
    names.reserve(notifiers_.size());
    for (const auto& notifier : notifiers_) {
        names.push_back(notifier->channel_name());
    }
    return names;
}

std::size_t NotificationDispatcher::size() const {
    return notifiers_.size();
}

std::vector<DispatchResult> NotificationDispatcher::dispatch(std::string_view message, const Recipients& recipients) {
    // Resolve every channel first so an unknown name sends nothing.
    std::vector<Notifier*> targets;
    targets.reserve(recipients.size());
    for (const auto& request : recipients) {
        const auto found = std::ranges::find_if(
            notifiers_, [&](const std::unique_ptr<Notifier>& n) { return n->channel_name() == request.first; });
        if (found == notifiers_.end()) {
            throw std::out_of_range("unknown channel: " + request.first);
        }
        targets.push_back(found->get());
    }

    std::vector<DispatchResult> results;
    results.reserve(recipients.size());
    for (std::size_t i = 0; i < recipients.size(); ++i) {
        Notifier& target = *targets[i];
        try {
            target.send(recipients[i].second, message);
            results.push_back(DispatchResult{std::string(target.channel_name()), nullptr});
        } catch (...) {
            results.push_back(DispatchResult{std::string(target.channel_name()), std::current_exception()});
        }
    }
    return results;
}

}  // namespace cppref::classes
