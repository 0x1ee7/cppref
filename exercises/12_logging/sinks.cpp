#include "sinks.hpp"

#include <stdexcept>

namespace cppref::logging {

std::string text_format(const Record&) {
    throw std::logic_error("not implemented: text_format");
}

std::string json_format(const Record&) {
    throw std::logic_error("not implemented: json_format");
}

std::string json_escape(std::string_view) {
    throw std::logic_error("not implemented: json_escape");
}

OstreamSink::OstreamSink(std::ostream&, Formatter) {
    throw std::logic_error("not implemented: OstreamSink");
}

void OstreamSink::write(const Record&) {
    throw std::logic_error("not implemented: OstreamSink::write");
}

void OstreamSink::flush() {
    throw std::logic_error("not implemented: OstreamSink::flush");
}

RotatingFileSink::RotatingFileSink(std::filesystem::path, std::uintmax_t, std::size_t, Formatter) {
    throw std::logic_error("not implemented: RotatingFileSink");
}

RotatingFileSink::~RotatingFileSink() = default;

void RotatingFileSink::write(const Record&) {
    throw std::logic_error("not implemented: RotatingFileSink::write");
}

void RotatingFileSink::flush() {
    throw std::logic_error("not implemented: RotatingFileSink::flush");
}

const std::filesystem::path& RotatingFileSink::path() const noexcept {
    static const std::filesystem::path empty;
    return empty;
}

}  // namespace cppref::logging
