// Part 1 - virtual interfaces: non-virtual interface (NVI), virtual destructors,
// override/final, a virtual call inside a constructor, and slicing.
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace cppref::polymorphism {

/// Abstract notification channel with a non-virtual public interface (NVI).
///
/// `send()` is the only entry point. It validates the message and then calls the
/// private pure virtual `do_send()`, which subclasses override. Subclasses never
/// override `send()`, so the base keeps its invariants.
///
/// Engineer: add the data members (sent counter, constructed-as tag).
class Channel {
public:
    /// Deleting through a Channel* must run the derived destructor.
    virtual ~Channel() = default;

    /// Throws std::invalid_argument for an empty message. Otherwise calls
    /// do_send() and increments sent_count() on success.
    void send(std::string_view message);

    /// Number of successful send() calls.
    [[nodiscard]] std::size_t sent_count() const;

    /// Kind tag of the concrete channel. The base implementation returns "channel".
    [[nodiscard]] virtual std::string_view kind() const;

    /// The value kind() returned while the Channel base subobject was being
    /// constructed. A virtual call in a constructor dispatches to the class that is
    /// currently under construction, so this is "channel" for every subclass.
    [[nodiscard]] std::string_view constructed_as() const;

    /// Polymorphic copy: a new heap object of the same dynamic type.
    [[nodiscard]] virtual std::unique_ptr<Channel> clone() const = 0;

protected:
    Channel();
    // Protected copy/move: only derived classes (and clone()) may copy a Channel,
    // so `Channel c = email;` cannot slice.
    Channel(const Channel&) = default;
    Channel& operator=(const Channel&) = default;
    Channel(Channel&&) = default;
    Channel& operator=(Channel&&) = default;

private:
    /// Subclass hook, called only from send() after validation.
    virtual void do_send(std::string_view message) = 0;
};

/// Email delivery. Keeps every delivered body in outbox(), in order.
class EmailChannel final : public Channel {
public:
    explicit EmailChannel(std::string address);

    [[nodiscard]] std::string_view kind() const override;  // "email"
    [[nodiscard]] std::unique_ptr<Channel> clone() const override;

    [[nodiscard]] const std::string& address() const;
    [[nodiscard]] const std::vector<std::string>& outbox() const;

private:
    void do_send(std::string_view message) override;
    // Engineer: data members.
};

/// Webhook delivery to an endpoint URL. Keeps every delivered body in outbox().
class WebhookChannel final : public Channel {
public:
    explicit WebhookChannel(std::string url);

    [[nodiscard]] std::string_view kind() const override;  // "webhook"
    [[nodiscard]] std::unique_ptr<Channel> clone() const override;

    [[nodiscard]] const std::string& url() const;
    [[nodiscard]] const std::vector<std::string>& outbox() const;

private:
    void do_send(std::string_view message) override;
    // Engineer: data members.
};

/// Notice base. Its copy operations are protected: an Alert cannot be sliced into
/// a Notice by value. Copy through clone() or keep a reference/pointer instead.
class Notice {
public:
    explicit Notice(std::string text);
    virtual ~Notice() = default;

    /// "notice: <text>".
    [[nodiscard]] virtual std::string render() const;

    /// Polymorphic copy of the dynamic type.
    [[nodiscard]] virtual std::unique_ptr<Notice> clone() const;

    [[nodiscard]] const std::string& text() const;

protected:
    Notice(const Notice&) = default;
    Notice& operator=(const Notice&) = default;
    Notice(Notice&&) = default;
    Notice& operator=(Notice&&) = default;
    // Engineer: data members.
};

/// Notice with a severity. render() is "alert[<severity>]: <text>".
class Alert final : public Notice {
public:
    Alert(std::string text, int severity);

    [[nodiscard]] std::string render() const override;
    [[nodiscard]] std::unique_ptr<Notice> clone() const override;

    [[nodiscard]] int severity() const;

    // Engineer: data members.
};

}  // namespace cppref::polymorphism
