#ifdef SOCKPUPPET_WITH_TLS

#include "error_code.h"

#include <openssl/err.h> // for ERR_error_string_n

#include <string> // for std::string

namespace {

struct ssl_error_code
{
  int error;

  ssl_error_code(int e) noexcept
    : error(e)
  {
  }
};

struct ssl_error_category : public std::error_category
{
  char const *name() const noexcept override
  {
    return "OpenSSLError";
  }

  std::string message(int messageId) const override
  {
    char buf[512];
    ::ERR_error_string_n(messageId, buf, sizeof(buf));
    return buf;
  }
};

ssl_error_category const &ssl_category()
{
  static ssl_error_category const c;
  return c;
}

std::error_code make_error_code(ssl_error_code const &se)
{
  return std::error_code(se.error, ssl_category());
}

} // unnamed namespace

namespace std {
  template<>
  struct is_error_code_enum<ssl_error_code> : std::true_type
  {
  };
} // namespace std

namespace sockpuppet {

std::error_code SslError(int code)
{
  return make_error_code(ssl_error_code(code));
}

} // namespace sockpuppet

#endif // SOCKPUPPET_WITH_TLS
