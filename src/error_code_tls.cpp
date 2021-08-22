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
    // print human-readable reason string
    if(auto reason = ERR_reason_error_string(static_cast<unsigned long>(messageId))) {
      return {reason};
    }

    // fall back to OpenSSL error code gibberish
    std::string buf;
    buf.reserve(256U);
    ERR_print_errors_cb([](char const *str, size_t len, void *buf) -> int {
      static_cast<std::string *>(buf)->append(str, len);
      return 1;
    }, &buf);
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
