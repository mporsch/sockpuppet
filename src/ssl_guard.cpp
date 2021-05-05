#ifdef WITH_TLS

#include "ssl_guard.h"

#include <openssl/ssl.h>

#include <mutex> // for std::mutex

namespace sockpuppet {

namespace {
  void UpdateInstanceCount(int modifier)
  {
    static std::mutex mtx;
    static int curr = 0;

    std::lock_guard<std::mutex> lock(mtx);

    auto const prev = curr;
    curr += modifier;

    if(prev == 0 && curr == 1) {
      // we are the first instance -> initialize
      SSL_library_init();
      SSL_load_error_strings();
      OpenSSL_add_ssl_algorithms();
      ERR_load_BIO_strings();
      ERR_load_SSL_strings();
    } else if(prev == 1 && curr == 0) {
      // we are the last instance -> cleanup
      // TODO
    }
  }
} // unnamed namespace

SslGuard::SslGuard()
{
  UpdateInstanceCount(1);
}

SslGuard::~SslGuard()
{
  UpdateInstanceCount(-1);
}

} // namespace sockpuppet

#endif // WITH_TLS
