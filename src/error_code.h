#ifndef SOCKPUPPET_UTIL_H
#define SOCKPUPPET_UTIL_H

#include <system_error> // for std::error_code

namespace sockpuppet {

// get error code for last socket operation
// must be called before other operations that may overwrite the cached value
// e.g. to_string(SocketAddress)
std::error_code SocketError();

std::error_code SocketError(int code);

std::error_code AddressError(int code);

} // namespace sockpuppet

#endif // SOCKPUPPET_UTIL_H
