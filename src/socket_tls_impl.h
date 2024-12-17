#ifndef SOCKPUPPET_SOCKET_TLS_IMPL_H
#define SOCKPUPPET_SOCKET_TLS_IMPL_H

#ifdef SOCKPUPPET_WITH_TLS

#include "socket_impl.h" // for SocketImpl
#include "ssl_guard.h" // for SslGuard

#include <openssl/ssl.h> // for SSL_CTX

#include <memory> // for std::unique_ptr
#include <string_view> // for std::string_view

namespace sockpuppet {

// the interface matches SocketImpl but some implicit differences exist:
//   may be readable but no user data can be received (only handshake data)
//   may be writable but no user data can be sent (handshake pending)
//   handshake data is sent/received on the socket during both send AND receive
//   if a send with limited timeout fails, it must be retried with the same data
//     (see https://www.openssl.org/docs/man1.1.1/man3/SSL_write.html)
struct SocketTlsImpl final : public SocketImpl
{
  struct SslDeleter
  {
    void operator()(SSL *ptr) const noexcept;
  };
  using SslPtr = std::unique_ptr<SSL, SslDeleter>;

  SslGuard sslGuard;  ///< Guard to initialize OpenSSL
  SslPtr ssl;  ///< OpenSSL session
  int lastError = SSL_ERROR_NONE;  ///< OpenSSL error cache
  std::string_view pendingSend;  ///< Buffer view to verify OpenSSL_write retry requirements
  Duration remainingTime;  ///< Use-case dependent timeout
  bool isReadable = false;  ///< Flag whether Driver has deemed us readable
  bool isWritable = false;  ///< Flag whether Driver has deemed us writable
  bool driverSendSuppressed = false;  ///< Flag whether Driver send polling was suppressed

  SocketTlsImpl(int family,
                int type,
                int protocol,
                char const *certFilePath,
                char const *keyFilePath);
  SocketTlsImpl(SOCKET fd, SSL_CTX *ctx);
  ~SocketTlsImpl() override;

  // waits for readable
  std::optional<size_t> Receive(char *data,
                                size_t size,
                                Duration timeout) override;
  // assumes a readable socket
  size_t Receive(char *data,
                 size_t size) override;

  // waits for writable (repeatedly if needed)
  size_t Send(char const *data,
              size_t size,
              Duration timeout) override;
  // assumes a writable socket
  size_t SendSome(char const *data,
                  size_t size) override;

  void Connect(SockAddrView const &connectAddr) override;

  void DriverQuery(short &events) override;
  void DriverPending() override;

  void Shutdown();
  size_t Read(char *data,
              size_t size);
  size_t BioRead(char *data,
                 size_t size);
  size_t Write(char const *data,
               size_t size);
  size_t BioWrite(char const *data,
                  size_t size);

  bool HandleResult(int res);
  bool HandleLastError();
  bool HandleError(int error);
};

struct AcceptorTlsImpl : public SocketImpl
{
  struct CtxDeleter
  {
    void operator()(SSL_CTX *ptr) const noexcept;
  };
  using CtxPtr = std::unique_ptr<SSL_CTX, CtxDeleter>;

  SslGuard sslGuard;  ///< Guard to initialize OpenSSL
  CtxPtr ctx;  ///< OpenSSL context to be shared with all accepted clients

  AcceptorTlsImpl(int family,
                  int type,
                  int protocol,
                  char const *certFilePath,
                  char const *keyFilePath);
  ~AcceptorTlsImpl() override;

  std::pair<SocketTcp, Address> Accept() override;
};

} // namespace sockpuppet

#endif // SOCKPUPPET_WITH_TLS

#endif // SOCKPUPPET_SOCKET_TLS_IMPL_H
