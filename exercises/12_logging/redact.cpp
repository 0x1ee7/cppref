#include "redact.hpp"

#include <stdexcept>

namespace cppref::logging {

bool is_secret_key(std::string_view) noexcept {
    return false;
}

std::string redact(std::string_view) {
    throw std::logic_error("not implemented: redact");
}

RedactingSink::RedactingSink(std::shared_ptr<Sink>) {
    throw std::logic_error("not implemented: RedactingSink");
}

void RedactingSink::write(const Record&) {
    throw std::logic_error("not implemented: RedactingSink::write");
}

void RedactingSink::flush() {
    throw std::logic_error("not implemented: RedactingSink::flush");
}

}  // namespace cppref::logging
