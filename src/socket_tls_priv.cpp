#ifdef WITH_TLS

#include "socket_tls_priv.h"
#include "error_code.h" // for SocketError
#include "wait.h" // for WaitReadableNonBlocking

#include <cassert> // for assert

namespace sockpuppet {

namespace {

static Duration const noTimeout(-1);
static Duration const noBlock(0);

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

  // dont block when sending long payloads
  (void)SSL_CTX_set_mode(ctx, SSL_MODE_ENABLE_PARTIAL_WRITE);
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
    SSL_set_fd(ssl.get(), static_cast<int>(fd));
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
  auto res = SSL_read(ssl, data, static_cast<int>(size));
  if(res < 0) {
    throw MakeSslError(ssl, res, "failed to TLS receive");
  } else if(res == 0) {
    throw std::runtime_error("TLS connection closed");
  }
  return static_cast<size_t>(res);
}

size_t DoSend(SSL *ssl, char const *data, size_t size)
{
  auto res = SSL_write(ssl, data, static_cast<int>(size));
  if(res < 0) {
    throw MakeSslError(ssl, res, "failed to TLS send");
  } else if((res == 0) && (size > 0U)) {
    throw std::logic_error("unexpected TLS send result");
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
  if(HandShake(timeout) &&
     WaitReadableBlocking(this->fd, timeout)) {
    return {DoReceive(ssl.get(), data, size)};
  }
  return {std::nullopt};
}

size_t SocketTlsClientPriv::Receive(char *data, size_t size)
{
  if(HandShake(noBlock)) {
    return DoReceive(ssl.get(), data, size);
  }
  return 0U; // socket was readable for TLS handshake only
}

size_t SocketTlsClientPriv::SendAll(char const *data, size_t size)
{
  size_t sent = 0U;
  if(HandShake(noTimeout)) {
    do {
      sent += DoSend(ssl.get(), data + sent, size - sent);
    } while(sent < size);
  }
  assert(sent == size);
  return sent;
}

size_t SocketTlsClientPriv::SendSome(
    char const *data, size_t size, Duration timeout)
{
  size_t sent = 0U;

  DeadlineLimited deadline(timeout);
  if(HandShake(timeout)) {
    deadline.Tick();
    do {
      if(!WaitWritableBlocking(fd, deadline.Remaining())) {
        break; // timeout exceeded
      }
      sent += DoSend(ssl.get(), data + sent, size - sent);
      deadline.Tick();
    } while((sent < size) && deadline.TimeLeft());
  }

  return sent;
}

size_t SocketTlsClientPriv::SendSome(char const *data, size_t size)
{
  if(HandShake(noBlock)) {
    return DoSend(ssl.get(), data, size);
  }
  return 0U; // socket was writable for TLS handshake only
}

void SocketTlsClientPriv::Connect(SockAddrView const &connectAddr)
{
  SocketPriv::Connect(connectAddr);
  SocketPriv::SetSockOptNonBlocking();
  SSL_set_connect_state(ssl.get());
  (void)HandShake(noBlock); // initiate the handshake
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
  (void)clientTls->HandShake(noBlock); // initiate the handshake

  return {std::move(clientTls), std::move(addr)};
}

} // namespace sockpuppet

#endif // WITH_TLS
