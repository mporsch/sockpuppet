#ifdef WITH_TLS

#include "socket_tls_priv.h"
#include "error_code.h" // for SocketError
#include "wait.h" // for WaitReadableNonBlocking

namespace sockpuppet {

namespace {

void ConfigureContext(SSL_CTX *ctx,
    char const *certFilePath, char const *keyFilePath)
{
  SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
  SSL_CTX_set_ecdh_auto(ctx, 1);

  if(SSL_CTX_use_certificate_file(ctx, certFilePath, SSL_FILETYPE_PEM) <= 0) {
    throw std::runtime_error("failed to set certificate");
  }
  if(SSL_CTX_use_PrivateKey_file(ctx, keyFilePath, SSL_FILETYPE_PEM) <= 0) {
    throw std::runtime_error("failed to set private key");
  }
}

SocketTlsServerPriv::CtxPtr CreateContext(SSL_METHOD const *method,
    char const *certFilePath, char const *keyFilePath)
{
  if(auto ctx = SocketTlsServerPriv::CtxPtr(SSL_CTX_new(method), SSL_CTX_free)) {
    ConfigureContext(ctx.get(), certFilePath, keyFilePath);
    return ctx;
  }
  throw std::runtime_error("failed to create SSL context");
}

void FreeSsl(SSL *ssl)
{
  if(ssl) {
      SSL_shutdown(ssl);
      SSL_free(ssl);
  }
}

SocketTlsClientPriv::SslPtr CreateSsl(SSL_CTX *ctx, SOCKET fd)
{
  if(auto ssl = SocketTlsClientPriv::SslPtr(SSL_new(ctx), FreeSsl)) {
    SSL_set_fd(ssl.get(), fd);
    return ssl;
  }
  throw std::runtime_error("failed to create SSL structure");
}

std::system_error MakeSslError(SSL *ssl, int code, char const *errorMessage)
{
  return std::system_error(
        SslError(SSL_get_error(ssl, code)),
        errorMessage);
}

size_t DoReceive(SSL *ssl, char *data, size_t size)
{
  auto const res = SSL_read(ssl, data, size);
  if(res < 0) {
    throw MakeSslError(ssl, res, "failed to TLS receive");
  } else if(res == 0) {
    throw std::runtime_error("connection closed");
  }
  return static_cast<size_t>(res);
}

} // unnamed namespace

SocketTlsClientPriv::SocketTlsClientPriv(int family, int type, int protocol,
    char const *certFilePath, char const *keyFilePath)
  : SocketPriv(family, type, protocol)
  , sslGuard()
  , ssl(CreateSsl(
          CreateContext(TLS_client_method(), certFilePath, keyFilePath).get(),
          this->fd))
  , isHandShakeComplete(false)
{
}

SocketTlsClientPriv::SocketTlsClientPriv(SocketPriv &&sock, SSL_CTX *ctx)
  : SocketPriv(std::move(sock))
  , sslGuard()
  , ssl(CreateSsl(ctx, this->fd))
  , isHandShakeComplete(false)
{
}

SocketTlsClientPriv::~SocketTlsClientPriv() = default;

std::optional<size_t> SocketTlsClientPriv::Receive(
    char *data, size_t size, Duration timeout)
{
  if(HandShake(timeout)) {
    return {DoReceive(ssl.get(), data, size)};
  }
  return {std::nullopt};
}

size_t SocketTlsClientPriv::Receive(char *data, size_t size)
{
  if(HandShake(Duration(0))) {
    return DoReceive(ssl.get(), data, size);
  }
  return 0; // socket was readable for TLS handshake only
}

size_t SocketTlsClientPriv::SendAll(char const *data, size_t size)
{
  return SendSome(data, size, Duration(-1));
}

size_t SocketTlsClientPriv::SendSome(
    char const *data, size_t size, Duration timeout)
{
  if(HandShake(timeout)) {
    auto const res = SSL_write(ssl.get(), data, size);
    if(res < 0) {
      throw MakeSslError(ssl.get(), res, "failed to TLS send");
    } else if((res == 0) && (size > 0U)) {
      throw std::logic_error("unexpected send result");
    }
    return static_cast<size_t>(res);
  }
  return 0;
}

size_t SocketTlsClientPriv::SendSome(char const *data, size_t size)
{
  return SendSome(data, size, Duration(0));
}

void SocketTlsClientPriv::Connect(SockAddrView const &connectAddr)
{
  SocketPriv::Connect(connectAddr);
  SocketPriv::SetSockOptNonBlocking();
  SSL_set_connect_state(ssl.get());
  (void)HandShake(Duration(0)); // initiate the handshake
}

bool SocketTlsClientPriv::HandShake(Duration timeout)
{
  if(isHandShakeComplete) {
    return true;
  }

  // execute TLS handshake while keeping track of the time
  return (timeout.count() >= 0 ?
      HandShakeLoop(DeadlineLimited(timeout)) :
      HandShakeLoop(DeadlineUnlimited()));
}

template<typename Deadline>
bool SocketTlsClientPriv::HandShakeLoop(Deadline deadline)
{
  do {
    auto res = SSL_do_handshake(ssl.get());
    deadline.Tick();
    if(res == 1) {
      isHandShakeComplete = true;
      SocketPriv::SetSockOptBlocking();
      return deadline.TimeLeft(); // run sockets if time remains
    }

    auto code = SSL_get_error(ssl.get(), res);
    if(!HandShakeWait(code, deadline.Remaining())) {
      return false;
    }
    deadline.Tick();
  } while(deadline.TimeLeft());
  return false;
}

bool SocketTlsClientPriv::HandShakeWait(int code, Duration timeout)
{
  switch(code) {
  case SSL_ERROR_WANT_READ:
    return WaitReadableNonBlocking(this->fd, timeout);
  case SSL_ERROR_WANT_WRITE:
    return WaitWritableNonBlocking(this->fd, timeout);
  default:
    throw MakeSslError(ssl.get(), code, "failed to do TLS handshake");
  }
}


SocketTlsServerPriv::SocketTlsServerPriv(int family, int type, int protocol,
    char const *certFilePath, char const *keyFilePath)
  : SocketPriv(family, type, protocol)
  , sslGuard()
  , ctx(CreateContext(TLS_server_method(), certFilePath, keyFilePath))
{
}

SocketTlsServerPriv::~SocketTlsServerPriv() = default;

std::pair<std::unique_ptr<SocketPriv>, Address>
SocketTlsServerPriv::Accept()
{
  auto [client, addr] = SocketPriv::Accept();
  client->SetSockOptNonBlocking();

  auto clientTls = std::make_unique<SocketTlsClientPriv>(
      std::move(*client),
      ctx.get());
  SSL_set_accept_state(clientTls->ssl.get());
  (void)clientTls->HandShake(Duration(0)); // initiate the handshake

  return {std::move(clientTls), std::move(addr)};
}

} // namespace sockpuppet

#endif // WITH_TLS
