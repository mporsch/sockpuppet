#ifndef _WIN32

#include "error_code.h"

#include <cerrno> // for errno

namespace sockpuppet {

std::error_code SocketError()
{
  return SocketError(errno);
}

std::error_code SocketError(int code)
{
  return std::error_code(code, std::system_category());
}

} // namespace sockpuppet

#endif // _WIN32
