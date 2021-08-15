#ifdef SOCKPUPPET_WITH_TLS

#include "socket_tls_priv.h"
#include "error_code.h" // for SslError
#include "wait.h" // for WaitReadableNonBlocking

#include <cassert> // for assert
#include <csignal> // for std::signal
#include <stdexcept> // for std::logic_error

namespace sockpuppet {

namespace {

static Duration const noBlock(0);

struct IgnoreSigPipeGuard
{
  IgnoreSigPipeGuard()
  {
#ifndef SO_NOSIGPIPE
# ifdef SIGPIPE
    // in Linux (where no other SIGPIPE workaround with OpenSSL
    // is available) ignore signal once for each thread we run in
    struct Shared
    {
      Shared()
      {
        // avoid SIGPIPE on connection closed
        // which might occur during SSL_write() or SSL_do_handshake()
        if(std::signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
          throw std::logic_error("failed to ignore SIGPIPE");
        }
      }
    };

    thread_local Shared const instance;
# endif // SIGPIPE
#endif // SO_NOSIGPIPE
  }
};

void ConfigureContext(SSL_CTX *ctx,
    char const *certFilePath, char const *keyFilePath)
{
  static int const flags =
      SSL_MODE_ENABLE_PARTIAL_WRITE | // dont block when sending long payloads
      SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER; // cannot guarantee user reuses buffer after timeout

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

} // unnamed namespace

SocketTlsClientPriv::SocketTlsClientPriv(int family, int type, int protocol,
    char const *certFilePath, char const *keyFilePath)
  : SocketPriv(family, type, protocol)
  , sslGuard()
  , ssl(CreateSsl(
          CreateContext(TLS_client_method(), certFilePath, keyFilePath).get(),
          this->fd))
{
}

SocketTlsClientPriv::SocketTlsClientPriv(SocketPriv &&sock, SSL_CTX *ctx)
  : SocketPriv(std::move(sock))
  , sslGuard()
  , ssl(CreateSsl(ctx, this->fd))
{
}

SocketTlsClientPriv::~SocketTlsClientPriv() = default;

std::optional<size_t> SocketTlsClientPriv::Receive(
    char *data, size_t size, Duration timeout)
{
  if(timeout.count() >= 0) {
    if(auto received = Receive(data, size, DeadlineLimited(timeout))) {
      return {received};
    } else {
      return {std::nullopt};
    }
  } else {
    auto received = Receive(data, size, DeadlineUnlimited());
    assert(received > 0U);
    return {received};
  }
}

size_t SocketTlsClientPriv::Receive(char *data, size_t size)
{
  return Receive(data, size, DeadlineLimited(noBlock));
}

template<typename Deadline>
size_t SocketTlsClientPriv::Receive(char *data, size_t size,
    Deadline &&deadline)
{
  IgnoreSigPipeGuard const guard;

  // run in a loop, as OpenSSL might require a handshake at any time
  for(;;) {
    auto res = SSL_read(ssl.get(), data, static_cast<int>(size));
    if(res < 0) {
      if(!Wait(SSL_get_error(ssl.get(), res), deadline.Remaining())) {
        return 0U; // timeout / socket was readable for TLS handshake only
      }
      deadline.Tick();
    } else if(res == 0) {
      throw std::runtime_error("TLS connection closed");
    } else {
      return static_cast<size_t>(res);
    }
  }
}

size_t SocketTlsClientPriv::Send(char const *data, size_t size,
    Duration timeout)
{
  return (timeout.count() < 0 ?
            SendAll(data, size) :
            SendSome(data, size, DeadlineLimited(timeout)));
}

size_t SocketTlsClientPriv::SendAll(char const *data, size_t size)
{
  auto sent = SendSome(data, size, DeadlineUnlimited());
  assert(sent == size);
  return sent;
}

template<typename Deadline>
size_t SocketTlsClientPriv::SendSome(char const *data, size_t size,
    Deadline &&deadline)
{
  IgnoreSigPipeGuard const guard;
  size_t sent = 0U;

  // run in a loop, as OpenSSL might require a handshake at any time
  do {
    auto res = SSL_write(ssl.get(), data + sent, static_cast<int>(size - sent));
    if(res < 0) {
      if(!Wait(SSL_get_error(ssl.get(), res), deadline.Remaining())) {
        break; // timeout / socket was writable for TLS handshake only
      }
      deadline.Tick();
    } else if((res == 0) && (size > 0U)) {
      throw std::logic_error("unexpected TLS send result");
    } else {
      sent += static_cast<size_t>(res);
    }
  } while(sent < size);

  return sent;
}

size_t SocketTlsClientPriv::SendSome(char const *data, size_t size)
{
  return SendSome(data, size, DeadlineLimited(noBlock));
}

void SocketTlsClientPriv::Connect(SockAddrView const &connectAddr)
{
  SocketPriv::Connect(connectAddr);
  SocketPriv::SetSockOptNonBlocking();

  SSL_set_connect_state(ssl.get());
  IgnoreSigPipeGuard const guard;
  (void)SSL_do_handshake(ssl.get()); // initiate the handshake
}

bool sockpuppet::SocketTlsClientPriv::WaitReadable(Duration timeout)
{
  return WaitReadableNonBlocking(this->fd, timeout);
}

bool SocketTlsClientPriv::WaitWritable(Duration timeout)
{
  return WaitWritableNonBlocking(this->fd, timeout);
}

bool SocketTlsClientPriv::Wait(int code, Duration timeout)
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
  IgnoreSigPipeGuard const guard;
  (void)SSL_do_handshake(clientTls->ssl.get()); // initiate the handshake

  return {std::move(clientTls), std::move(addr)};
}

} // namespace sockpuppet

#endif // SOCKPUPPET_WITH_TLS
