// This file is part of Asteria.
// Copyleft 2018 - 2020, LH_Mouse. All wrongs reserved.

#ifndef ASTERIA_LIBRARY_CHRONO_HPP_
#define ASTERIA_LIBRARY_CHRONO_HPP_

#include "../fwd.hpp"

namespace Asteria {

// `std.chrono.utc_now`
V_integer
std_chrono_utc_now();

// `std.chrono.local_now`
V_integer
std_chrono_local_now();

// `std.chrono.hires_now`
V_real
std_chrono_hires_now();

// `std.chrono.steady_now`
V_integer
std_chrono_steady_now();

// `std.chrono.local_from_utc`
V_integer
std_chrono_local_from_utc(V_integer time_utc);

// `std.chrono.utc_from_local`
V_integer
std_chrono_utc_from_local(V_integer time_local);

// `std.chrono.utc_format`
V_string
std_chrono_utc_format(V_integer time_point, optV_boolean with_ms);

// `std.chrono.utc_parse`
V_integer
std_chrono_utc_parse(V_string time_str);

// Initialize an object that is to be referenced as `std.chrono`.
void
create_bindings_chrono(V_object& result, API_Version version);

}  // namespace Asteria

#endif
