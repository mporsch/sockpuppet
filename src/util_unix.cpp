#ifndef _WIN32

#include "util.h"

std::error_code LastError()
{
  return std::error_code(errno, std::system_category());
}

} // namespace sockpuppet

#endif // _WIN32
