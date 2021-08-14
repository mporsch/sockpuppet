#ifndef SOCKPUPPET_SOCKET_TLS_PRIV_H
#define SOCKPUPPET_SOCKET_TLS_PRIV_H

#ifdef SOCKPUPPET_WITH_TLS

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
  bool isHandShakeComplete;  ///< during TLS handshake the socket is set to non-blocking mode

  SocketTlsClientPriv(int family,
                      int type,
                      int protocol,
                      char const *certFilePath,
                      char const *keyFilePath);
  SocketTlsClientPriv(SocketPriv &&sock, SSL_CTX *ctx);
  ~SocketTlsClientPriv() override;

  // waits for readable
  std::optional<size_t> Receive(char *data,
                                size_t size,
                                Duration timeout) override;
  // assumes a readable socket
  size_t Receive(char *data,
                 size_t size) override;

  size_t SendAll(char const *data,
                 size_t size) override;
  // waits for writable repeatedly and
  // sends the max amount of data within the user-provided timeout
  size_t SendSome(char const *data,
                  size_t size,
                  Duration timeout) override;
  // assumes a writable socket
  size_t SendSome(char const *data,
                  size_t size) override;

  void Connect(SockAddrView const &connectAddr) override;

  bool WaitReadable(Duration timeout) override;
  bool WaitWritable(Duration timeout) override;

  // return whether TLS handshake was performed and if time remains for data send/receive
  template<typename Deadline>
  bool HandShake(Deadline &&deadline);
  bool HandShakeWait(int code, Duration timeout);
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

#endif // SOCKPUPPET_WITH_TLS

#endif // SOCKPUPPET_SOCKET_TLS_PRIV_H
