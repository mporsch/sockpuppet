#ifndef SOCKPUPPET_SOCKET_TLS_PRIV_H
#define SOCKPUPPET_SOCKET_TLS_PRIV_H

#ifdef WITH_TLS

#include "address_priv.h" // for SockAddrView
#include "socket_priv.h" // for SocketPriv
#include "ssl_guard.h" // for SslGuard

#include <openssl/ssl.h> // for SSL_CTX

#include <cstddef> // for size_t
#include <memory> // for std::unique_ptr
#include <utility> // for std::pair

namespace sockpuppet {

struct SocketTlsClientPriv : public SocketPriv
{
  using SslPtr = std::unique_ptr<SSL, void (*)(SSL *)>;

  SslGuard sslGuard;  ///< Guard to initialize OpenSSL
  SslPtr ssl;  ///< OpenSSL session

  SocketTlsClientPriv(int family,
                      int type,
                      int protocol,
                      char const *certFilePath,
                      char const *keyFilePath);
  SocketTlsClientPriv(SocketPriv &&sock, SSL_CTX *ctx);
  ~SocketTlsClientPriv() override;

  size_t Receive(char *data,
                 size_t size) override;

  size_t SendAll(char const *data,
                 size_t size) override;
  size_t SendSome(char const *data,
                  size_t size) override;

  void Connect(SockAddrView const &connectAddr) override;
};

struct SocketTlsServerPriv : public SocketPriv
{
  // context is reference-counted by itself: used for transport to sessions only
  using CtxPtr = std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)>;

  SslGuard sslGuard;  ///< Guard to initialize OpenSSL
  CtxPtr ctx;  ///< OpenSSL context to be shared with all accepted clients

  SocketTlsServerPriv(int family,
                      int type,
                      int protocol,
                      char const *certFilePath,
                      char const *keyFilePath);
  ~SocketTlsServerPriv() override;

  std::pair<std::unique_ptr<SocketPriv>, Address>
  Accept() override;
};

} // namespace sockpuppet

#endif // WITH_TLS

#endif // SOCKPUPPET_SOCKET_TLS_PRIV_H
