#ifndef SOCKPUPPET_SOCKET_TLS_PRIV_H
#define SOCKPUPPET_SOCKET_TLS_PRIV_H

#include "address_priv.h" // for SockAddrView
#include "socket_priv.h" // for SocketPriv
#include "ssl_guard.h" // for SslGuard

#include <openssl/ssl.h> // for SSL_CTX

#include <cstddef> // for size_t
#include <memory> // for std::unique_ptr
#include <utility> // for std::pair

namespace sockpuppet {

struct SocketTlsPriv : public SocketPriv
{
  using SslPtr = std::unique_ptr<SSL, void (*)(SSL *)>;

  SslGuard sslGuard;  ///< Guard to initialize OpenSSL
  SslPtr ssl;

  SocketTlsPriv(int family,
                int type,
                int protocol,
                SSL_METHOD const *method,
                char const *certFilePath,
                char const *keyFilePath);

  SocketTlsPriv(SOCKET fd, SSL_CTX *ctx);

  ~SocketTlsPriv() override;

  size_t Receive(char *data,
                 size_t size) override;

  size_t SendAll(char const *data,
                 size_t size) override;
  size_t SendSome(char const *data,
                  size_t size) override;

  void Connect(SockAddrView const &connectAddr) override;

  std::pair<SocketTcpClient, Address>
  Accept() override;
};

} // namespace sockpuppet

#endif // SOCKPUPPET_SOCKET_TLS_PRIV_H
