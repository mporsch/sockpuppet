#ifndef SOCKPUPPET_ERROR_CODE_H
#define SOCKPUPPET_ERROR_CODE_H

#include <system_error> // for std::error_code

namespace sockpuppet {

// get error code for last socket operation
// must be called before other operations that may overwrite the cached value
// e.g. to_string(Address)
std::error_code SocketError();

std::error_code SocketError(int code);

std::error_code AddressError(int code);

#ifdef SOCKPUPPET_WITH_TLS
std::error_code SslError(int code);
#endif // SOCKPUPPET_WITH_TLS

} // namespace sockpuppet

#endif // SOCKPUPPET_ERROR_CODE_H
