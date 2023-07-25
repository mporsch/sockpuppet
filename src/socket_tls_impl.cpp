#ifdef SOCKPUPPET_WITH_TLS

#include "socket_tls_impl.h"
#include "error_code.h" // for SocketError
#include "wait.h" // for WaitReadableNonBlocking
#include <iostream>
#include <cassert> // for assert
#include <csignal> // for std::signal
#include <stdexcept> // for std::logic_error

namespace sockpuppet {

namespace {

// loop over OpenSSL calls that might perform a handshake at any time
// the number is arbitrary and used only to avoid/detect infinite loops
constexpr int handshakeStepsMax = 10;

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
  constexpr int flags =
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

AcceptorTlsImpl::CtxPtr CreateCtx(SSL_METHOD const *method,
    char const *certFilePath, char const *keyFilePath)
{
  if(auto ctx = AcceptorTlsImpl::CtxPtr(SSL_CTX_new(method))) {
    ConfigureCtx(ctx.get(), certFilePath, keyFilePath);
    return ctx;
  }
  throw std::runtime_error("failed to create SSL context");
}

SocketTlsImpl::SslPtr CreateSsl(SSL_CTX *ctx, SOCKET fd)
{
  if(auto ssl = SocketTlsImpl::SslPtr(SSL_new(ctx))) {
    SSL_set_fd(ssl.get(), static_cast<int>(fd));
    return ssl;
  }
  throw std::runtime_error("failed to create SSL structure");
}

} // unnamed namespace

void SocketTlsImpl::SslDeleter::operator()(SSL *ptr) const noexcept
{
  SSL_free(ptr);
}

SocketTlsImpl::SocketTlsImpl(int family, int type, int protocol,
    char const *certFilePath, char const *keyFilePath)
  : SocketImpl(family, type, protocol)
  , sslGuard()
  , ssl(CreateSsl( // context is reference-counted by itself -> free temporary handle
          CreateCtx(TLS_client_method(), certFilePath, keyFilePath).get(),
          this->fd))
  , lastError(SSL_ERROR_NONE)
  , pendingSend(nullptr)
{
}

SocketTlsImpl::SocketTlsImpl(SocketImpl &&sock, SSL_CTX *ctx)
  : SocketImpl(std::move(sock))
  , sslGuard()
  , ssl(CreateSsl(ctx, this->fd))
  , lastError(SSL_ERROR_NONE)
  , pendingSend(nullptr)
{
}

SocketTlsImpl::~SocketTlsImpl()
{
  switch(lastError) {
  case SSL_ERROR_SYSCALL:
  case SSL_ERROR_SSL:
    break;
  default:
    try {
      Shutdown();
    } catch(...) {
    }
    break;
  }
}

std::optional<size_t> SocketTlsImpl::Receive(
    char *data, size_t size, Duration timeout)
{
  // unlimited timeout performs full handshake and subsequent receive
  auto received =
      (timeout.count() < 0 ?
         Receive(data, size, DeadlineUnlimited()) :
         (timeout.count() == 0 ?
            Receive(data, size, DeadlineZero()) :
            Receive(data, size, DeadlineLimited(timeout))));

  if(received) {
    return {received};
  }

  assert(timeout.count() >= 0);
  return {std::nullopt};
}

size_t SocketTlsImpl::Receive(char *data, size_t size)
{
  // we have been deemed readable
  if(lastError == SSL_ERROR_WANT_READ) {
    lastError = SSL_ERROR_NONE;
  }

  return Receive(data, size, DeadlineZero());
}

template<typename Deadline>
size_t SocketTlsImpl::Receive(char *data, size_t size,
    Deadline deadline)
{
  IgnoreSigPipe();

  if(HandleLastError(deadline)) {
    for(int i = 1; i <= handshakeStepsMax; ++i) {
      auto res = SSL_read(ssl.get(), data, static_cast<int>(size));
      std::cout << this << " SSL_read -> ";
      if(res < 0) {
        if(!HandleResult(res, deadline)) {
          break;
        }
      } else if(res == 0) {
        throw std::runtime_error("TLS connection closed");
      } else {
        std::cout << "OK" << std::endl;
        return static_cast<size_t>(res);
      }
      assert(i < handshakeStepsMax);
    }
  }
  return 0U; // timeout / socket was readable for TLS handshake only
}

size_t SocketTlsImpl::Send(char const *data, size_t size,
    Duration timeout)
{
  return (timeout.count() < 0 ?
            SendAll(data, size) :
            (timeout.count() == 0 ?
               SendSome(data, size, DeadlineZero()) :
               SendSome(data, size, DeadlineLimited(timeout))));
}

size_t SocketTlsImpl::SendAll(char const *data, size_t size)
{
  auto sent = SendSome(data, size, DeadlineUnlimited());
  assert(sent == size);
  return sent;
}

template<typename Deadline>
size_t SocketTlsImpl::SendSome(char const *data, size_t size,
    Deadline deadline)
{
  IgnoreSigPipe();

  if(pendingSend) {
    assert(pendingSend == data);
    pendingSend = nullptr;
  }

  size_t sent = 0U;
  if(HandleLastError(deadline)) {
    for(int i = 1; (i <= handshakeStepsMax) && (sent < size); ++i) {
      size_t written = 0U;
      auto res = SSL_write_ex(ssl.get(), data + sent, size - sent, &written);
      std::cout << this << " SSL_write(" << (void*)data << ") -> ";
      if(res <= 0) {
        if(!HandleResult(res, deadline)) {
          pendingSend = data;
          break; // timeout / socket was writable for TLS handshake only
        }
      } else {
        std::cout << "OK" << std::endl;
        sent += written;
      }
      assert(i < handshakeStepsMax);
    }
  }
  return sent;
}

size_t SocketTlsImpl::SendSome(char const *data, size_t size)
{
  // we have been deemed writable/readable
  if(lastError == SSL_ERROR_WANT_WRITE ||
     (pendingSend && lastError == SSL_ERROR_WANT_READ)) {
    lastError = SSL_ERROR_NONE;
  }

  return SendSome(data, size, DeadlineZero());
}

void SocketTlsImpl::Connect(SockAddrView const &connectAddr)
{
  SocketImpl::Connect(connectAddr);
  SocketImpl::SetSockOptNonBlocking();

  SSL_set_connect_state(ssl.get());
}

void SocketTlsImpl::Shutdown()
{
  IgnoreSigPipe();

  if(SSL_shutdown(ssl.get()) <= 0) {
    char buf[32];
    DeadlineLimited deadline(std::chrono::seconds(1));
    for(int i = 1; i <= handshakeStepsMax; ++i) {
      auto res = SSL_read(ssl.get(), buf, sizeof(buf));
      std::cout << this << " SSL_shutdown -> ";
      if(res < 0) {
        if(!HandleResult(res, deadline)) {
          break;
        }
      } else if(res == 0) {
        std::cout << "OK" << std::endl;
        break;
      }
      assert(i < handshakeStepsMax);
    }

    (void)SSL_shutdown(ssl.get());
  }
}

bool sockpuppet::SocketTlsImpl::WaitReadable(Duration timeout)
{
  return WaitReadableNonBlocking(this->fd, timeout);
}

bool SocketTlsImpl::WaitWritable(Duration timeout)
{
  return WaitWritableNonBlocking(this->fd, timeout);
}

template<typename Deadline>
bool SocketTlsImpl::HandleResult(int ret, Deadline &deadline)
{
  auto error = SSL_get_error(ssl.get(), ret);
  std::cout << error << std::endl;
  if(HandleError(error, deadline)) {
    lastError = SSL_ERROR_NONE;
    return true;
  }
  lastError = error;
  return false;
}

template<typename Deadline>
bool SocketTlsImpl::HandleLastError(Deadline &deadline)
{
  if(HandleError(lastError, deadline)) {
    lastError = SSL_ERROR_NONE;
    return true;
  }
  return false;
}

template<typename Deadline>
bool SocketTlsImpl::HandleError(int error, Deadline &deadline)
{
  constexpr char errorMessage[] =
      "failed to wait for TLS socket readable/writable";

  switch(error) {
  case SSL_ERROR_NONE:
    return true;
  case SSL_ERROR_WANT_READ:
    if(WaitReadableNonBlocking(this->fd, deadline.Remaining())) {
      deadline.Tick(); // assume that in non-blocking mode everything except Wait is instantaneous
      return true;
    }
    return false;
  case SSL_ERROR_WANT_WRITE:
    if(WaitWritableNonBlocking(this->fd, deadline.Remaining())) {
      deadline.Tick(); // assume that in non-blocking mode everything except Wait is instantaneous
      return true;
    }
    return false;
  case SSL_ERROR_SYSCALL:
    throw std::system_error(SocketError(), errorMessage);
  case SSL_ERROR_ZERO_RETURN:
    throw std::runtime_error(errorMessage);
  case SSL_ERROR_SSL:
    [[fallthrough]];
  default:
    throw std::system_error(SslError(error), errorMessage);
  }
}


void AcceptorTlsImpl::CtxDeleter::operator()(SSL_CTX *ptr) const noexcept
{
  SSL_CTX_free(ptr);
}

AcceptorTlsImpl::AcceptorTlsImpl(int family, int type, int protocol,
    char const *certFilePath, char const *keyFilePath)
  : SocketImpl(family, type, protocol)
  , sslGuard()
  , ctx(CreateCtx(TLS_server_method(), certFilePath, keyFilePath))
{
}

AcceptorTlsImpl::~AcceptorTlsImpl() = default;

std::pair<SocketTcp, Address>
AcceptorTlsImpl::Accept()
{
  auto [client, addr] = SocketImpl::Accept();
  client.impl->SetSockOptNonBlocking();

  auto clientTls = std::make_unique<SocketTlsImpl>(
      std::move(*client.impl),
      ctx.get());

  SSL_set_accept_state(clientTls->ssl.get());

  return {SocketTcp(std::move(clientTls)), std::move(addr)};
}

} // namespace sockpuppet

#endif // SOCKPUPPET_WITH_TLS
