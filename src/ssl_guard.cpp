#ifdef WITH_TLS

#include "ssl_guard.h"

#include <openssl/ssl.h> // for SSL_library_init

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
      (void)SSL_library_init();
      SSL_load_error_strings();
      // TODO OPENSSL_config / OPENSSL_noconfig
      // no multithreading within the library, so no OpenSSL thread init
    } else if(prev == 1 && curr == 0) {
      // we are the last instance -> cleanup
      EVP_cleanup();
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
