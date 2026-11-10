#pragma once

#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

namespace cppref::error_handling {

// Every error in this exercise derives from DomainError, which derives from
// std::runtime_error. A generic `catch (const std::exception&)` therefore still
// sees a readable what().

/// Root of every domain error.
class DomainError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// Root of configuration failures. Catch this to handle all of them.
class ConfigError : public DomainError {
public:
    using DomainError::DomainError;
};

/// The config file does not exist. Message: "config file not found: <path>".
/// The OS-level cause (a std::system_error) is nested with std::throw_with_nested.
class ConfigNotFound : public ConfigError {
public:
    using ConfigError::ConfigError;
};

/// The path exists but cannot be read (a directory, I/O failure, ...).
/// Message: "cannot read config: <path>". Cause nested as for ConfigNotFound.
class ConfigUnreadable : public ConfigError {
public:
    using ConfigError::ConfigError;
};

/// The file was read but is not a valid config. Message: "<path>: line N: ...".
/// Cause: a ParseError, nested with std::throw_with_nested.
class ConfigInvalid : public ConfigError {
public:
    using ConfigError::ConfigError;
};

/// A dotted key is not defined. Carries the key as data, not only as text.
/// Message: "setting '<key>' is not defined". Raised with a plain throw, so no
/// nested cause exists.
class SettingMissing : public ConfigError {
public:
    explicit SettingMissing(std::string key);

    /// The dotted key that was looked up.
    [[nodiscard]] const std::string& key() const;

private:
    std::string key_;
};

/// what() of `e` and of every exception nested inside it, outermost first.
/// Walks std::nested_exception links set by std::throw_with_nested. A non-std
/// exception in the chain contributes "unknown exception" and ends the walk.
[[nodiscard]] std::vector<std::string> describe_chain(const std::exception& e);

}  // namespace cppref::error_handling
