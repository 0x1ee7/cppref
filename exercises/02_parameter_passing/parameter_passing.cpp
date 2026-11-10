// 02 · Parameter passing: non-template definitions (engineer-owned).
#include "parameter_passing.hpp"

namespace cppref::parameter_passing {

Method parse_method(std::string_view) {
    detail::not_implemented("parse_method");
}

std::string_view method_name(Method) {
    detail::not_implemented("method_name");
}

Request build_request(Method, std::string_view, RequestOptions) {
    detail::not_implemented("build_request");
}

Response Transport::send(const Request&, int) const {
    detail::not_implemented("Transport::send");
}

User apply_patch(User, const UserPatch&) {
    detail::not_implemented("apply_patch");
}

std::vector<std::string_view> split_fields(std::string_view, char) {
    detail::not_implemented("split_fields");
}

std::string join_fields(std::span<const std::string_view>, char) {
    detail::not_implemented("join_fields");
}

std::string format_value(bool) {
    detail::not_implemented("format_value(bool)");
}

std::string format_value(std::int64_t) {
    detail::not_implemented("format_value(int64_t)");
}

std::string format_value(double) {
    detail::not_implemented("format_value(double)");
}

std::string format_value(std::string_view) {
    detail::not_implemented("format_value(string_view)");
}

std::string format_value(const char*) {
    detail::not_implemented("format_value(const char*)");
}

}  // namespace cppref::parameter_passing
