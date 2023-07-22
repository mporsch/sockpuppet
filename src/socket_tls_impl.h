#ifndef SOCKPUPPET_SOCKET_TLS_IMPL_H
#define SOCKPUPPET_SOCKET_TLS_IMPL_H

#ifdef SOCKPUPPET_WITH_TLS

#include "address_impl.h" // for SockAddrView
#include "socket_impl.h" // for SocketImpl
#include "ssl_guard.h" // for SslGuard

#include <openssl/ssl.h> // for SSL_CTX

#include <cstddef> // for size_t
#include <memory> // for std::unique_ptr
#include <utility> // for std::pair

namespace sockpuppet {

// unlike the regular TCP client socket, the TLS one is set to non-blocking mode
// to maintain control of the timing behaviour during the TLS handshake
struct SocketTlsClientImpl : public SocketImpl
{
  struct SslDeleter
  {
    void operator()(SSL *ptr) const noexcept;
  };
  using SslPtr = std::unique_ptr<SSL, SslDeleter>;

  SslGuard sslGuard;  ///< Guard to initialize OpenSSL
  SslPtr ssl;  ///< OpenSSL session
  int lastError;  ///< OpenSSL error cache
  char const *pendingSend;  ///< flag to satisfy OpenSSL_write retry requirements

  SocketTlsClientImpl(int family,
                      int type,
                      int protocol,
                      char const *certFilePath,
                      char const *keyFilePath);
  SocketTlsClientImpl(SocketImpl &&sock, SSL_CTX *ctx);
  ~SocketTlsClientImpl() override;

  // waits for readable
  std::optional<size_t> Receive(char *data,
                                size_t size,
                                Duration timeout) override;
  // assumes a readable socket
  size_t Receive(char *data,
                 size_t size) override;
  template<typename Deadline>
  size_t Receive(char *data,
                 size_t size,
                 Deadline deadline);

  size_t Send(char const *data,
              size_t size,
              Duration timeout) override;
  size_t SendAll(char const *data,
                 size_t size);
  // waits for writable repeatedly and
  // sends the max amount of data within the user-provided timeout
  template<typename Deadline>
  size_t SendSome(char const *data,
                  size_t size,
                  Deadline deadline);
  // assumes a writable socket
  size_t SendSome(char const *data,
                  size_t size) override;
  template<typename Deadline>
  size_t Send(char const *data,
              size_t size,
              Deadline &&deadline);

  void Connect(SockAddrView const &connectAddr) override;

  void Shutdown();

  bool WaitReadable(Duration timeout) override;
  bool WaitWritable(Duration timeout) override;
  bool HandleError(int ret, Duration timeout);
  bool HandleLastError(Duration timeout);
  bool Wait(int error, Duration timeout);
};

struct SocketTlsServerImpl : public SocketImpl
{
  struct CtxDeleter
  {
    void operator()(SSL_CTX *ptr) const noexcept;
  };
  using CtxPtr = std::unique_ptr<SSL_CTX, CtxDeleter>;

  SslGuard sslGuard;  ///< Guard to initialize OpenSSL
  CtxPtr ctx;  ///< OpenSSL context to be shared with all accepted clients

  SocketTlsServerImpl(int family,
                      int type,
                      int protocol,
                      char const *certFilePath,
                      char const *keyFilePath);
  ~SocketTlsServerImpl() override;

  std::pair<SocketTcp, Address>
  Accept() override;
};

} // namespace sockpuppet

#endif // SOCKPUPPET_WITH_TLS

#endif // SOCKPUPPET_SOCKET_TLS_IMPL_H
