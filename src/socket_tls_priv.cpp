#ifdef SOCKPUPPET_WITH_TLS

#include "socket_tls_priv.h"
#include "error_code.h" // for SocketError
#include "wait.h" // for WaitReadableNonBlocking

#include <cassert> // for assert
#include <csignal> // for std::signal
#include <stdexcept> // for std::logic_error

namespace sockpuppet {

namespace {

static Duration const noTimeout(-1);
static Duration const noBlock(0);

struct IgnoreSigPipeGuard
{
  IgnoreSigPipeGuard()
  {
#ifndef SO_NOSIGPIPE
# ifdef SIGPIPE
    // in Linux (where no other SIGPIPE workaround with OpenSSL
    // is available) ignore signal for the whole program
    if(std::signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
      throw std::logic_error("failed to ignore SIGPIPE");
    }
# endif // SIGPIPE
#endif // SO_NOSIGPIPE
  }
};

void ConfigureContext(SSL_CTX *ctx,
    char const *certFilePath, char const *keyFilePath)
{
  // avoid SIGPIPE on connection closed
  // which might occur during SSL_write() or SSL_do_handshake()
  static IgnoreSigPipeGuard const ignoreSigPipe;

  static int const flags =
      SSL_MODE_ENABLE_PARTIAL_WRITE | // dont block when sending long payloads
      SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER | // TODO
      SSL_MODE_AUTO_RETRY; // TODO

  if((SSL_CTX_set_mode(ctx, flags) & flags) != flags) {
    throw std::logic_error("failed to set TLS mode");
  }

  if(SSL_CTX_set_ecdh_auto(ctx, 1) <= 0) {
    throw std::logic_error("failed to set TLS ECDH");
  }

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
  if(isHandShakeComplete) {
     if(WaitReadable(timeout)) {
       return {DoReceive(ssl.get(), data, size)};
     }
  } else {
    if(timeout.count() >= 0) {
      DeadlineLimited deadline(timeout);
      if(HandShake(deadline) && WaitReadable(deadline.Remaining())) {
        return {DoReceive(ssl.get(), data, size)};
      }
    } else {
      DeadlineUnlimited deadline;
      if(HandShake(deadline) && WaitReadable(deadline.Remaining())) {
        return {DoReceive(ssl.get(), data, size)};
      }
    }
  }
  return {std::nullopt};
}

size_t SocketTlsClientPriv::Receive(char *data, size_t size)
{
  if(isHandShakeComplete) {
    return DoReceive(ssl.get(), data, size);
  } else {
    (void)HandShake(DeadlineLimited(noBlock));
  }
  return 0U; // socket was readable for TLS handshake only
}

size_t SocketTlsClientPriv::SendAll(char const *data, size_t size)
{
  if(!isHandShakeComplete) {
    (void)HandShake(DeadlineUnlimited());
  }

  size_t sent = 0U;
  do {
    (void)WaitWritable(noTimeout);
    sent += DoSend(ssl.get(), data + sent, size - sent);
  } while(sent < size);
  assert(sent == size);
  return sent;
}

size_t SocketTlsClientPriv::SendSome(
    char const *data, size_t size, Duration timeout)
{
  size_t sent = 0U;

  DeadlineLimited deadline(timeout);
  if(isHandShakeComplete || HandShake(deadline)) {
    deadline.Tick();
    do {
      if(!WaitWritable(deadline.Remaining())) {
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
  if(!isHandShakeComplete) {
    (void)HandShake(DeadlineLimited(noBlock));
    return 0U; // socket was writable for TLS handshake only
  }
  return DoSend(ssl.get(), data, size);
}

void SocketTlsClientPriv::Connect(SockAddrView const &connectAddr)
{
  SocketPriv::Connect(connectAddr);
  SocketPriv::SetSockOptNonBlocking();
  SSL_set_connect_state(ssl.get());
  (void)HandShake(DeadlineLimited(noBlock)); // initiate the handshake
}

bool sockpuppet::SocketTlsClientPriv::WaitReadable(Duration timeout)
{
  return WaitReadableNonBlocking(this->fd, timeout);
}

bool SocketTlsClientPriv::WaitWritable(Duration timeout)
{
  return WaitWritableNonBlocking(this->fd, timeout);
}

template<typename Deadline>
bool SocketTlsClientPriv::HandShake(Deadline &&deadline)
{
  assert(!isHandShakeComplete);

  // execute TLS handshake while keeping track of the time
  do {
    auto res = SSL_do_handshake(ssl.get());
    if(res == 1) {
      isHandShakeComplete = true;
      return true;
    }

    auto code = SSL_get_error(ssl.get(), res);
    deadline.Tick();
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
    return WaitReadable(timeout);
  case SSL_ERROR_WANT_WRITE:
    return WaitWritable(timeout);
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
  (void)clientTls->HandShake(DeadlineLimited(noBlock)); // initiate the handshake

  return {std::move(clientTls), std::move(addr)};
}

} // namespace sockpuppet

#endif // SOCKPUPPET_WITH_TLS
