#ifdef SOCKPUPPET_WITH_TLS

#include "ssl_guard.h"

#include <openssl/crypto.h> // for CRYPTO_set_locking_callback
#include <openssl/ssl.h> // for SSL_library_init

#include <memory> // for std::unique_ptr
#include <mutex> // for std::mutex
#include <thread> // for std::this_thread::get_id
#include <stdexcept> // for std::logic_error

namespace sockpuppet {

namespace {

std::mutex &Mutex(size_t n)
{
  static auto const count = static_cast<size_t>(CRYPTO_num_locks());
  static std::unique_ptr<std::mutex[]> const mutices(new std::mutex[count]);

  if(n >= count) {
    throw std::logic_error("invalid SSL locking index");
  }
  return mutices[n];
}

void Locking(int mode, int n, char const *, int)
{
  if(mode & CRYPTO_LOCK) {
    Mutex(static_cast<size_t>(n)).lock();
  } else {
    Mutex(static_cast<size_t>(n)).unlock();
  }
}

unsigned long Id()
{
  std::hash<std::thread::id> hasher;
  return static_cast<unsigned long>(hasher(std::this_thread::get_id()));
}

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
    CRYPTO_set_id_callback(Id);
    CRYPTO_set_locking_callback(Locking);
  } else if(prev == 1 && curr == 0) {
    // we are the last instance -> cleanup
    CRYPTO_set_id_callback(nullptr);
    CRYPTO_set_locking_callback(nullptr);
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

#endif // SOCKPUPPET_WITH_TLS
