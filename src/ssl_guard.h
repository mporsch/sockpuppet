#ifndef SOCKPUPPET_SSL_GUARD_H
#define SOCKPUPPET_SSL_GUARD_H

namespace sockpuppet {

struct SslGuard
{
#ifdef WITH_TLS
  SslGuard();
  SslGuard(SslGuard const &other) = delete;
  SslGuard(SslGuard &&other) = delete;
  ~SslGuard();
  SslGuard &operator=(SslGuard const &other) = delete;
  SslGuard &operator=(SslGuard &&other) = delete;
#endif // WITH_TLS
};

} // namespace sockpuppet

#endif // SOCKPUPPET_SSL_GUARD_H
