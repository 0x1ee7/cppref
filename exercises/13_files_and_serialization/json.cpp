// Engineer-owned. Contract: json.hpp.
#include "json.hpp"

#include <stdexcept>

namespace cppref::files {

Json Json::null() { throw std::logic_error("not implemented: Json::null"); }
Json Json::boolean(bool) { throw std::logic_error("not implemented: Json::boolean"); }
Json Json::integer(std::int64_t) { throw std::logic_error("not implemented: Json::integer"); }
Json Json::string(std::string) { throw std::logic_error("not implemented: Json::string"); }
Json Json::array(Array) { throw std::logic_error("not implemented: Json::array"); }
Json Json::object(Object) { throw std::logic_error("not implemented: Json::object"); }

Json::Kind Json::kind() const noexcept { return Kind::null; }
bool Json::as_bool() const { throw std::logic_error("not implemented: Json::as_bool"); }
std::int64_t Json::as_integer() const { throw std::logic_error("not implemented: Json::as_integer"); }
const std::string& Json::as_string() const { throw std::logic_error("not implemented: Json::as_string"); }
const Json::Array& Json::as_array() const { throw std::logic_error("not implemented: Json::as_array"); }
const Json::Object& Json::as_object() const { throw std::logic_error("not implemented: Json::as_object"); }
const Json* Json::find(std::string_view) const { throw std::logic_error("not implemented: Json::find"); }
std::string Json::dump(int) const { throw std::logic_error("not implemented: Json::dump"); }
Json Json::parse(std::string_view) { throw std::logic_error("not implemented: Json::parse"); }

}  // namespace cppref::files
