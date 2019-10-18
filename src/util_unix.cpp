#ifndef _WIN32

#include "util.h"

#include <cerrno> // for errno

namespace sockpuppet {

std::error_code LastError()
{
  return std::error_code(errno, std::system_category());
}

} // namespace sockpuppet

#endif // _WIN32
