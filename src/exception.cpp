// This file is part of asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "exception.hpp"

namespace Asteria {

Exception::Exception(Exception &&) = default;
Exception &Exception::operator=(Exception &&) = default;
Exception::~Exception() = default;

}
