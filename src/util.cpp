#include "util.h"

#ifdef _WIN32
# include <ws2tcpip.h> // for gai_strerror
#else
# include <netdb.h> // for gai_strerror
#endif // _WIN32

namespace {

struct gai_error_code
{
  int error;

  gai_error_code(int e) noexcept
    : error(e)
  {
  }
};

struct gai_error_category : public std::error_category
{
  char const *name() const noexcept override
  {
    return "GetAddrInfoError";
  }

  std::string message(int messageId) const override
  {
    return ::gai_strerror(messageId);
  }
};

gai_error_category const &gai_category()
{
  static gai_error_category const c;
  return c;
}

std::error_code make_error_code(gai_error_code const &ge)
{
  return std::error_code(ge.error, gai_category());
}

} // unnamed namespace

namespace std {

template<>
struct is_error_code_enum<gai_error_code> : std::true_type
{
};

} // namespace std

namespace sockpuppet {

std::error_code AddressError(int code)
{
  return make_error_code(gai_error_code(code));
}

} // namespace sockpuppet
