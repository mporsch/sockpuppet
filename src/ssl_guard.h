#ifndef SOCKPUPPET_SSL_GUARD_H
#define SOCKPUPPET_SSL_GUARD_H

#ifdef SOCKPUPPET_WITH_TLS

namespace sockpuppet {

struct SslGuard
{
  SslGuard();
  SslGuard(SslGuard const &other) = delete;
  SslGuard(SslGuard &&other) = delete;
  ~SslGuard();
  SslGuard &operator=(SslGuard const &other) = delete;
  SslGuard &operator=(SslGuard &&other) = delete;
};

} // namespace sockpuppet

#endif // SOCKPUPPET_WITH_TLS

#endif // SOCKPUPPET_SSL_GUARD_H
