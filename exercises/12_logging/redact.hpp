// Secret redaction for messages and structured fields.
#pragma once

#include "logging.hpp"

#include <memory>
#include <string>
#include <string_view>

namespace cppref::logging {

// Secret keys (case-insensitive, whole words only): password, passwd, secret,
// token, api_key, api-key, access_token, client_secret, authorization.
bool is_secret_key(std::string_view key) noexcept;

// Masks the value of every secret key found in free text. The key, separator
// and quotes are kept; the value becomes "***":
//   password=hunter2             -> password=***
//   token: abc                   -> token: ***
//   secret='a b c'               -> secret='***'
//   api_key: "xyz"               -> api_key: "***"
//   Authorization: Basic xyz     -> Authorization: ***   (rest of line)
//   {'Authorization': 'Bearer x'} -> {'Authorization': '***'}
// Unquoted values end at whitespace, ',', '&', '}' or ';'; the Authorization
// value is unquoted-to-end-of-line. Words that merely contain a key
// ("tokens-bucket", "secretary=ok") are left alone.
std::string redact(std::string_view text);

// Rewrites each record's message through redact() and masks fields whose key
// is a secret key. Forwards every record (never drops) to the target sink.
// Attach it to the loggers or handlers whose output must be scrubbed, such as
// a parent logger, so that child loggers are covered too.
class RedactingSink final : public Sink {
public:
    explicit RedactingSink(std::shared_ptr<Sink> target);

    void write(const Record& record) override;
    void flush() override;

    // engineer: members (target sink).
};

}  // namespace cppref::logging
