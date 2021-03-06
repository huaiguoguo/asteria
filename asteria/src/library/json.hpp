// This file is part of Asteria.
// Copyleft 2018 - 2020, LH_Mouse. All wrongs reserved.

#ifndef ASTERIA_LIBRARY_JSON_HPP_
#define ASTERIA_LIBRARY_JSON_HPP_

#include "../fwd.hpp"

namespace Asteria {

// `std.json.format`
V_string
std_json_format(Value value, optV_string indent);

V_string
std_json_format(Value value, V_integer indent);

// `std.json.format5`
V_string
std_json_format5(Value value, optV_string indent);

V_string
std_json_format5(Value value, V_integer indent);

// `std.json.parse`
Value
std_json_parse(V_string text);

// `std.json.parse_file`
Value
std_json_parse_file(V_string path);

// Create an object that is to be referenced as `std.json`.
void
create_bindings_json(V_object& result, API_Version version);

}  // namespace Asteria

#endif
