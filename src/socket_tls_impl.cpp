#ifdef SOCKPUPPET_WITH_TLS

#include "socket_tls_impl.h"
#include "error_code.h" // for SocketError
#include "wait.h" // for WaitReadableNonBlocking

#include <cassert> // for assert
#include <csignal> // for std::signal
#include <stdexcept> // for std::logic_error

namespace sockpuppet {

namespace {

// loop over OpenSSL calls that might perform a handshake at any time
// the number is arbitrary and used only to avoid/detect infinite loops
static int const handshakeStepsMax = 10;

void IgnoreSigPipe()
{
#ifndef SO_NOSIGPIPE
# ifdef SIGPIPE
  struct PerThread
  {
    PerThread()
    {
      // avoid SIGPIPE on connection closed
      // which might occur during SSL_write() or SSL_do_handshake()
      if(std::signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        throw std::logic_error("failed to ignore SIGPIPE");
      }
    }
  };

  // in Linux (where no other SIGPIPE workaround with OpenSSL
  // is available) ignore signal once for each thread we run in
  thread_local PerThread const instance;
# endif // SIGPIPE
#endif // SO_NOSIGPIPE
}

void ConfigureCtx(SSL_CTX *ctx,
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

SocketTlsServerImpl::CtxPtr CreateCtx(SSL_METHOD const *method,
    char const *certFilePath, char const *keyFilePath)
{
  if(auto ctx = SocketTlsServerImpl::CtxPtr(SSL_CTX_new(method))) {
    ConfigureCtx(ctx.get(), certFilePath, keyFilePath);
    return ctx;
  }
  throw std::runtime_error("failed to create SSL context");
}

SocketTlsClientImpl::SslPtr CreateSsl(SSL_CTX *ctx, SOCKET fd)
{
  if(auto ssl = SocketTlsClientImpl::SslPtr(SSL_new(ctx))) {
    SSL_set_fd(ssl.get(), static_cast<int>(fd));
    return ssl;
  }
  throw std::runtime_error("failed to create SSL structure");
}

} // unnamed namespace

void SocketTlsClientImpl::SslDeleter::operator()(SSL *ptr) const noexcept
{
  SSL_free(ptr);
}

SocketTlsClientImpl::SocketTlsClientImpl(int family, int type, int protocol,
    char const *certFilePath, char const *keyFilePath)
  : SocketImpl(family, type, protocol)
  , sslGuard()
  , ssl(CreateSsl( // context is reference-counted by itself -> free temporary handle
          CreateCtx(TLS_client_method(), certFilePath, keyFilePath).get(),
          this->fd))
  , properShutdown(true)
{
}

SocketTlsClientImpl::SocketTlsClientImpl(SocketImpl &&sock, SSL_CTX *ctx)
  : SocketImpl(std::move(sock))
  , sslGuard()
  , ssl(CreateSsl(ctx, this->fd))
  , properShutdown(true)
{
}

SocketTlsClientImpl::~SocketTlsClientImpl()
{
  if(properShutdown) {
    try {
      Shutdown();
    } catch(...) {
    }
  }
}

std::optional<size_t> SocketTlsClientImpl::Receive(
    char *data, size_t size, Duration timeout)
{
  auto received =
      (timeout.count() < 0 ?
         Receive(data, size, DeadlineUnlimited()) :
         (timeout.count() == 0 ?
            Receive(data, size, DeadlineZero()) :
            Receive(data, size, DeadlineLimited(timeout))));

  // unlimited timeout performs full handshake and subsequent receive
  assert((received > 0U) || (timeout.count() >= 0));

  if(!received) {
    return {std::nullopt};
  }
  return {received};
}

size_t SocketTlsClientImpl::Receive(char *data, size_t size)
{
  return Receive(data, size, DeadlineZero());
}

template<typename Deadline>
size_t SocketTlsClientImpl::Receive(char *data, size_t size,
    Deadline deadline)
{
  IgnoreSigPipe();

  for(int i = 1; i <= handshakeStepsMax; ++i) {
    auto res = SSL_read(ssl.get(), data, static_cast<int>(size));
    if(res < 0) {
      if(!Wait(res, deadline.Remaining())) {
        break;
      } // assume that in non-blocking mode everything except Wait is instantaneous
      deadline.Tick();
    } else if(res == 0) {
      throw std::runtime_error("TLS connection closed");
    } else {
      return static_cast<size_t>(res);
    }
    assert(i < handshakeStepsMax);
  }
  return 0U; // timeout / socket was readable for TLS handshake only
}

size_t SocketTlsClientImpl::Send(char const *data, size_t size,
    Duration timeout)
{
  return (timeout.count() < 0 ?
            SendAll(data, size) :
            (timeout.count() == 0 ?
               SendSome(data, size, DeadlineZero()) :
               SendSome(data, size, DeadlineLimited(timeout))));
}

size_t SocketTlsClientImpl::SendAll(char const *data, size_t size)
{
  auto sent = SendSome(data, size, DeadlineUnlimited());
  assert(sent == size);
  return sent;
}

template<typename Deadline>
size_t SocketTlsClientImpl::SendSome(char const *data, size_t size,
    Deadline deadline)
{
  IgnoreSigPipe();

  size_t sent = 0U;
  for(int i = 1; (i <= handshakeStepsMax) && (sent < size); ++i) {
    auto res = SSL_write(ssl.get(), data + sent, static_cast<int>(size - sent));
    if(res < 0) {
      if(!Wait(res, deadline.Remaining())) {
        break; // timeout / socket was writable for TLS handshake only
      } // assume that in non-blocking mode everything except Wait is instantaneous
      deadline.Tick();
    } else if((res == 0) && (size > 0U)) {
      throw std::logic_error("unexpected TLS send result");
    } else {
      sent += static_cast<size_t>(res);
    }
    assert(i < handshakeStepsMax);
  }
  return sent;
}

size_t SocketTlsClientImpl::SendSome(char const *data, size_t size)
{
  return SendSome(data, size, DeadlineZero());
}

void SocketTlsClientImpl::Connect(SockAddrView const &connectAddr)
{
  SocketImpl::Connect(connectAddr);
  SocketImpl::SetSockOptNonBlocking();

  SSL_set_connect_state(ssl.get());
  IgnoreSigPipe();
  (void)SSL_do_handshake(ssl.get()); // initiate the handshake
}

void SocketTlsClientImpl::Shutdown()
{
  IgnoreSigPipe();

  if(SSL_shutdown(ssl.get()) <= 0) {
    char buf[32];
    DeadlineLimited deadline(std::chrono::seconds(1));
    for(int i = 1; i <= handshakeStepsMax; ++i) {
      auto res = SSL_read(ssl.get(), buf, sizeof(buf));
      if(res < 0) {
        if(!Wait(res, deadline.Remaining())) {
          break;
        } // assume that in non-blocking mode everything except Wait is instantaneous
        deadline.Tick();
      } else if(res == 0) {
        break;
      }
      assert(i < handshakeStepsMax);
    }

    (void)SSL_shutdown(ssl.get());
  }
}

bool sockpuppet::SocketTlsClientImpl::WaitReadable(Duration timeout)
{
  return WaitReadableNonBlocking(this->fd, timeout);
}

bool SocketTlsClientImpl::WaitWritable(Duration timeout)
{
  return WaitWritableNonBlocking(this->fd, timeout);
}

bool SocketTlsClientImpl::Wait(int code, Duration timeout)
{
  static char const errorMessage[] =
      "failed to wait for TLS socket readable/writable";

  switch(SSL_get_error(ssl.get(), code)) {
  case SSL_ERROR_WANT_READ:
    return WaitReadableNonBlocking(this->fd, timeout);
  case SSL_ERROR_WANT_WRITE:
    return WaitWritableNonBlocking(this->fd, timeout);
  case SSL_ERROR_SYSCALL:
    // SSL_shutdown must not be called after SSL_ERROR_SYSCALL
    properShutdown = false;
    throw std::system_error(SocketError(), errorMessage);
  case SSL_ERROR_ZERO_RETURN:
    throw std::runtime_error(errorMessage);
  case SSL_ERROR_SSL:
    // SSL_shutdown must not be called after SSL_ERROR_SSL
    properShutdown = false;
    [[fallthrough]];
  default:
    assert(code != SSL_ERROR_NONE);
    throw std::system_error(SslError(code), errorMessage);
  }
}


void SocketTlsServerImpl::CtxDeleter::operator()(SSL_CTX *ptr) const noexcept
{
  SSL_CTX_free(ptr);
}

SocketTlsServerImpl::SocketTlsServerImpl(int family, int type, int protocol,
    char const *certFilePath, char const *keyFilePath)
  : SocketImpl(family, type, protocol)
  , sslGuard()
  , ctx(CreateCtx(TLS_server_method(), certFilePath, keyFilePath))
{
}

SocketTlsServerImpl::~SocketTlsServerImpl() = default;

std::pair<SocketTcp, Address>
SocketTlsServerImpl::Accept()
{
  auto [client, addr] = SocketImpl::Accept();
  client.impl->SetSockOptNonBlocking();

  auto clientTls = std::make_unique<SocketTlsClientImpl>(
      std::move(*client.impl),
      ctx.get());

  SSL_set_accept_state(clientTls->ssl.get());
  IgnoreSigPipe();
  (void)SSL_do_handshake(clientTls->ssl.get()); // initiate the handshake

  return {SocketTcp(std::move(clientTls)), std::move(addr)};
}

} // namespace sockpuppet

#endif // SOCKPUPPET_WITH_TLS
