#ifdef SOCKPUPPET_WITH_TLS

#include "ssl_guard.h"

#include <openssl/crypto.h> // for CRYPTO_set_locking_callback
#include <openssl/ssl.h> // for SSL_library_init

#include <atomic> // for std::atomic
#include <memory> // for std::unique_ptr
#include <mutex> // for std::mutex
#include <thread> // for std::this_thread::get_id
#include <stdexcept> // for std::logic_error

struct CRYPTO_dynlock_value
{
  std::mutex mtx;
};

namespace {

CRYPTO_dynlock_value *DynlockCreate(char const *, int)
{
  return new CRYPTO_dynlock_value;
}

void DynlockLock(int mode, CRYPTO_dynlock_value *v, char const *, int)
{
  if(mode & CRYPTO_LOCK) {
    v->mtx.lock();
  } else {
    v->mtx.unlock();
  }
}

void DynlockDestroy(CRYPTO_dynlock_value *v, char const *, int)
{
  delete v;
}

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

std::atomic<int> &Count()
{
  static std::atomic<int> curr = 0;
  return curr;
}

void Dec() noexcept
{
  auto prev = Count().fetch_add(-1);
  if(prev == 1) {
    // we are the last instance -> cleanup
    CRYPTO_set_dynlock_destroy_callback(nullptr);
    CRYPTO_set_dynlock_lock_callback(nullptr);
    CRYPTO_set_dynlock_create_callback(nullptr);
    CRYPTO_set_locking_callback(nullptr);
    CRYPTO_set_id_callback(nullptr);
    EVP_cleanup();
  }
}

void Inc()
{
  auto prev = Count().fetch_add(1);
  if(prev == 0) {
    // we are the first instance -> initialize
    (void)SSL_library_init();
    SSL_load_error_strings();
    CRYPTO_set_id_callback(Id);
    CRYPTO_set_locking_callback(Locking);
    CRYPTO_set_dynlock_create_callback(DynlockCreate);
    CRYPTO_set_dynlock_lock_callback(DynlockLock);
    CRYPTO_set_dynlock_destroy_callback(DynlockDestroy);
  }
}

} // unnamed namespace

namespace sockpuppet {

SslGuard::SslGuard()
{
  Inc();
}

SslGuard::~SslGuard() noexcept
{
  Dec();
}

} // namespace sockpuppet

#endif // SOCKPUPPET_WITH_TLS
