// Part 7: Notifier ABC and NotificationDispatcher — interface, ownership and exception capture.
#pragma once

#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cppref::classes {

/// Webhook delivery failed: the endpoint answered with a status outside 200-299.
class NotificationError : public std::runtime_error {
public:
    NotificationError(const std::string& message, int status) : std::runtime_error(message), status_(status) {}

    [[nodiscard]] int status() const noexcept { return status_; }

private:
    int status_;
};

/// Interface. Failures are reported by throwing: std::invalid_argument for bad recipients,
/// NotificationError for HTTP failures, and whatever the transport throws (propagated unchanged).
class Notifier {
public:
    virtual ~Notifier() = default;

    [[nodiscard]] virtual std::string_view channel_name() const = 0;
    virtual void send(std::string_view recipient, std::string_view message) = 0;

    /// "<channel_name> notifier".
    [[nodiscard]] std::string describe() const;
};

/// transport(address, body)
class EmailNotifier final : public Notifier {
public:
    using Transport = std::function<void(std::string_view address, std::string_view body)>;

    explicit EmailNotifier(Transport transport);

    [[nodiscard]] std::string_view channel_name() const override;
    void send(std::string_view recipient, std::string_view message) override;

private:
    Transport transport_;
};

/// transport(number, text)
class SMSNotifier final : public Notifier {
public:
    static constexpr std::size_t kMaxLength = 160;

    using Transport = std::function<void(std::string_view number, std::string_view text)>;

    explicit SMSNotifier(Transport transport);

    [[nodiscard]] std::string_view channel_name() const override;
    void send(std::string_view recipient, std::string_view message) override;

private:
    Transport transport_;
};

/// post(endpoint, body) returns the HTTP status code.
class WebhookNotifier final : public Notifier {
public:
    using Post = std::function<int(std::string_view endpoint, std::string_view body)>;

    /// Throws std::invalid_argument unless endpoint starts with "https://".
    WebhookNotifier(std::string endpoint, Post post);

    [[nodiscard]] std::string_view channel_name() const override;
    void send(std::string_view recipient, std::string_view message) override;

private:
    std::string endpoint_;
    Post post_;
};

/// Outcome for one requested channel. `error` is null on success.
struct DispatchResult {
    std::string channel;
    std::exception_ptr error;
};

/// Owns its notifiers. Channel names must be unique.
class NotificationDispatcher {
public:
    /// Ordered (channel, recipient) pairs. Order is preserved in the results.
    using Recipients = std::vector<std::pair<std::string, std::string>>;

    /// Throws std::invalid_argument on a null notifier or duplicate channel name.
    explicit NotificationDispatcher(std::vector<std::unique_ptr<Notifier>> notifiers);

    [[nodiscard]] std::vector<std::string_view> channels() const;
    [[nodiscard]] std::size_t size() const;

    /// Validates every channel name before sending anything: an unknown channel
    /// throws std::out_of_range with no sends. Otherwise sends to each requested channel;
    /// a std::exception from a channel is captured in its result and the loop continues.
    [[nodiscard]] std::vector<DispatchResult> dispatch(std::string_view message, const Recipients& recipients);

private:
    std::vector<std::unique_ptr<Notifier>> notifiers_;
};

}  // namespace cppref::classes
