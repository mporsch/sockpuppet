#ifndef SOCKPUPPET_SOCKET_TLS_IMPL_H
#define SOCKPUPPET_SOCKET_TLS_IMPL_H

#ifdef SOCKPUPPET_WITH_TLS

#include "address_impl.h" // for SockAddrView
#include "socket_impl.h" // for SocketImpl
#include "ssl_guard.h" // for SslGuard

#include <openssl/bio.h> // for BIO
#include <openssl/ssl.h> // for SSL_CTX

#include <cstddef> // for size_t
#include <memory> // for std::unique_ptr
#include <string>
#include <utility> // for std::pair

namespace sockpuppet {

struct SocketTlsImpl : public SocketImpl
{
  struct SslDeleter
  {
    void operator()(SSL *ptr) const noexcept;
  };
  using SslPtr = std::unique_ptr<SSL, SslDeleter>;

  SslGuard sslGuard;  ///< Guard to initialize OpenSSL
  BIO *rbio; ///< SSL reads from, we write to
  BIO *wbio; ///< SSL writes to, we read from
  SslPtr ssl;  ///< OpenSSL session
  int lastError;  ///< OpenSSL error cache
  char const *pendingSend;  ///< flag to satisfy OpenSSL_write retry requirements
  std::string buffer;
  Duration pendingTimeout;

  SocketTlsImpl(int family,
                int type,
                int protocol,
                char const *certFilePath,
                char const *keyFilePath);
  SocketTlsImpl(SocketImpl &&sock, SSL_CTX *ctx);
  ~SocketTlsImpl() override;

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

  void Connect(SockAddrView const &connectAddr) override;

  void Shutdown();

  template<typename Deadline>
  bool HandleResult(int ret, Deadline &deadline);
  template<typename Deadline>
  bool HandleLastError(Deadline &deadline);
  template<typename Deadline>
  bool HandleError(int error, Deadline &deadline);

  template<typename Deadline>
  size_t SendPending(Deadline &deadline);
  template<typename Deadline>
  bool ReceiveIncoming(Deadline &deadline);
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
