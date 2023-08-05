#ifndef SOCKPUPPET_SOCKET_TLS_IMPL_H
#define SOCKPUPPET_SOCKET_TLS_IMPL_H

#ifdef SOCKPUPPET_WITH_TLS

#include "socket_impl.h" // for SocketImpl
#include "ssl_guard.h" // for SslGuard
#include "wait.h" // for Deadline*

#include <openssl/ssl.h> // for SSL_CTX

#include <memory> // for std::unique_ptr
#include <variant> // for std::variant

namespace sockpuppet {

struct SocketTlsImpl : public SocketImpl
{
  struct SslDeleter
  {
    void operator()(SSL *ptr) const noexcept;
  };
  using SslPtr = std::unique_ptr<SSL, SslDeleter>;

  SslGuard sslGuard;  ///< Guard to initialize OpenSSL
  SslPtr ssl;  ///< OpenSSL session
  int lastError = SSL_ERROR_NONE;  ///< OpenSSL error cache
  char const *pendingSend = nullptr;  ///< flag to satisfy OpenSSL_write retry requirements
  std::variant<DeadlineUnlimited, DeadlineZero, DeadlineLimited> deadline;  ///< use-case dependent deadline type

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

  size_t Send(char const *data,
              size_t size,
              Duration timeout) override;
  // assumes a writable socket
  size_t SendSome(char const *data,
                  size_t size) override;

  void Connect(SockAddrView const &connectAddr) override;

  size_t Read(char *data,
              size_t size,
              Duration timeout);
  // waits for writable repeatedly and
  // sends the max amount of data within the user-provided timeout
  size_t Write(char const *data,
               size_t size,
               Duration timeout);
  void Shutdown();

  void SetDeadline(Duration timeout);

  template<typename Fn>
  auto UnderDeadline(Fn &&fn) -> auto
  {
    return std::visit([&fn](auto &&deadline) -> auto {
      auto res = fn(deadline.Remaining());
      deadline.Tick();
      return res;
    }, deadline);
  }

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
