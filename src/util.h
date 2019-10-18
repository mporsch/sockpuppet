#ifndef SOCKPUPPET_UTIL_H
#define SOCKPUPPET_UTIL_H

#include <system_error> // for std::error_code

namespace sockpuppet {

// get error code for last socket operation
// must be called before to_string() to avoid premature reset
std::error_code LastError();

} // namespace sockpuppet

#endif // SOCKPUPPET_UTIL_H
