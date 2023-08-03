#ifdef SOCKPUPPET_WITH_TLS

#include "socket_tls_impl.h"
#include "error_code.h" // for SocketError
#include "wait.h" // for Deadline*

#include <cassert> // for assert
#include <stdexcept> // for std::logic_error

namespace sockpuppet {

namespace {

// loop over OpenSSL calls that might perform a handshake at any time
// the number is arbitrary and used only to avoid/detect infinite loops
constexpr int handshakeStepsMax = 10;

constexpr size_t DEFAULT_BUFFER_SIZE = 4096U;

namespace bio {

int write(BIO *b, char const *data, int s)
{
  auto *sock = reinterpret_cast<SocketTlsImpl *>(BIO_get_data(b));
  auto &&fd = sock->fd;
  auto size = static_cast<size_t>(s);
  auto &&timeout = sock->pendingTimeout;

  auto sent =
      (timeout.count() < 0 ?
         SendSome(fd, data, size, DeadlineUnlimited()) :
         (timeout.count() == 0 ?
            SendSome(fd, data, size, DeadlineZero()) :
            SendSome(fd, data, size, DeadlineLimited(timeout))));

  if(sent > 0U) {
    BIO_clear_retry_flags(b);
  } else {
    BIO_set_retry_write(b);
  }

  return static_cast<int>(sent);
}

int read(BIO *b, char *data, int size)
{
  auto *sock = reinterpret_cast<SocketTlsImpl *>(BIO_get_data(b));

  if(auto received = Receive(sock->fd, data, static_cast<size_t>(size), sock->pendingTimeout)) {
    BIO_clear_retry_flags(b);
    return static_cast<int>(*received);
  }
  BIO_set_retry_read(b);
  return 0;
}

//int puts(BIO *b, char const *data)
//{
//  return write(b, data, static_cast<int>(strlen(data)));
//}
//
//int gets(BIO *b, char *data, int size)
//{
//  auto *sock = reinterpret_cast<SocketTlsImpl *>(BIO_get_data(b));
//  return static_cast<int>(sock->Receive(data, static_cast<size_t>(size)));
//}

long ctrl(BIO *b, int cmd, long larg, void *parg)
{
  auto *sock = reinterpret_cast<SocketTlsImpl *>(BIO_get_data(b));

  switch(cmd) {
  case BIO_CTRL_PENDING:
    return 0;
  case BIO_CTRL_FLUSH:
    break;
  }
  return 1;
}

int create(BIO *b)
{
  BIO_set_init(b, 1);
  return 1;
}

//int destroy(BIO *b)
//{
//  auto *sock = reinterpret_cast<SocketTlsImpl *>(BIO_get_data(b));
//  return 1;
//}
//
//long callback_ctrl(BIO *b, int, BIO_info_cb *)
//{
//  auto *sock = reinterpret_cast<SocketTlsImpl *>(BIO_get_data(b));
//}

} // namespace bio

struct BioMethodDeleter
{
  void operator()(BIO_METHOD *ptr) const noexcept
  {
    BIO_meth_free(ptr);
  }
};
using BioMethodPtr = std::unique_ptr<BIO_METHOD, BioMethodDeleter>;

BIO_METHOD *BIO_s_sockpuppet()
{
  static auto instance = []() -> BioMethodPtr {
    auto method = BioMethodPtr(BIO_meth_new(
        BIO_get_new_index() | BIO_TYPE_SOURCE_SINK,
        "sockpuppet"));
    if(true &&
       BIO_meth_set_write(method.get(), bio::write) &&
       BIO_meth_set_read(method.get(), bio::read) &&
       //BIO_meth_set_puts(method.get(), bio::puts) &&
       //BIO_meth_set_gets(method.get(), bio::gets) &&
       BIO_meth_set_ctrl(method.get(), bio::ctrl) &&
       BIO_meth_set_create(method.get(), bio::create) &&
       //BIO_meth_set_destroy(method.get(), bio::destroy) &&
       //BIO_meth_set_callback_ctrl(method.get(), bio::callback_ctrl) &&
       true) {
      return method;
    }
    throw std::logic_error("failed to create BIO method");
  }();
  return instance.get();
}

struct BioDeleter
{
  void operator()(BIO *ptr) const noexcept
  {
    (void)BIO_free(ptr);
  }
};
using BioPtr = std::unique_ptr<BIO, BioDeleter>;

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

SocketTlsImpl::SslPtr CreateSsl(SSL_CTX *ctx, BioPtr rbio, BioPtr wbio)
{
  if(auto ssl = SocketTlsImpl::SslPtr(SSL_new(ctx))) {
    SSL_set_bio(ssl.get(), rbio.release(), wbio.release());
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
  , sslGuard() // must be created before call to SSL_CTX_new
  , rbio(BIO_new(BIO_s_sockpuppet()))
  , wbio(BIO_new(BIO_s_sockpuppet()))
  , ssl(CreateSsl( // context is reference-counted by itself -> free temporary handle
          CreateCtx(TLS_client_method(), certFilePath, keyFilePath).get(),
          BioPtr(rbio), BioPtr(wbio)))
  , lastError(SSL_ERROR_NONE)
  , pendingSend(nullptr)
  , buffer(DEFAULT_BUFFER_SIZE,  '\0')
  , pendingTimeout(-1)
{
  BIO_set_data(rbio, this);
  BIO_set_data(wbio, this);
}

SocketTlsImpl::SocketTlsImpl(SocketImpl &&sock, SSL_CTX *ctx)
  : SocketImpl(std::move(sock))
  , sslGuard()
  , rbio(BIO_new(BIO_s_sockpuppet()))
  , wbio(BIO_new(BIO_s_sockpuppet()))
  , ssl(CreateSsl(ctx, BioPtr(rbio), BioPtr(wbio)))
  , lastError(SSL_ERROR_NONE)
  , pendingSend(nullptr)
  , buffer(DEFAULT_BUFFER_SIZE,  '\0')
  , pendingTimeout(-1)
{
  BIO_set_data(rbio, this);
  BIO_set_data(wbio, this);
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
  pendingTimeout = timeout;

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
  if(HandleLastError(deadline)) {
    for(int i = 1; i <= handshakeStepsMax; ++i) {
      auto res = SSL_read(ssl.get(), data, static_cast<int>(size));
      if(res < 0) {
        if(!HandleResult(res, deadline)) {
          break;
        }
      } else if(res == 0) {
        throw std::runtime_error("TLS connection closed");
      } else {
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
  pendingTimeout = timeout;

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
  if(pendingSend) {
    assert(pendingSend == data);
    pendingSend = nullptr;
  }

  size_t sent = 0U;
  if(HandleLastError(deadline)) {
    for(int i = 1; (i <= handshakeStepsMax) && (sent < size); ++i) {
      size_t written = 0U;
      auto res = SSL_write_ex(ssl.get(), data + sent, size - sent, &written);
      if(res <= 0) {
        if(!HandleResult(res, deadline)) {
          pendingSend = data;
          break; // timeout / socket was writable for TLS handshake only
        }
      } else {
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

  SSL_set_connect_state(ssl.get());
}

void SocketTlsImpl::Shutdown()
{
  if(SSL_shutdown(ssl.get()) <= 0) {
    char buf[32];
    DeadlineLimited deadline(std::chrono::seconds(1));
    for(int i = 1; i <= handshakeStepsMax; ++i) {
      auto res = SSL_read(ssl.get(), buf, sizeof(buf));
      if(res < 0) {
        if(!HandleResult(res, deadline)) {
          break;
        }
      } else if(res == 0) {
        break;
      }
      assert(i < handshakeStepsMax);
    }

    (void)SSL_shutdown(ssl.get());
  }
}

template<typename Deadline>
bool SocketTlsImpl::HandleResult(int ret, Deadline &deadline)
{
  auto error = SSL_get_error(ssl.get(), ret);
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
  constexpr char errorMessage[] = "failed to TLS receive/send/handshake";

  switch(error) {
  case SSL_ERROR_NONE:
    return true;
  case SSL_ERROR_WANT_READ:
    (void)SendPending(deadline);
    return ReceiveIncoming(deadline);
  case SSL_ERROR_WANT_WRITE:
    return (SendPending(deadline) > 0U);
  case SSL_ERROR_SSL:
    throw std::system_error(SslError(error), errorMessage);
  case SSL_ERROR_SYSCALL:
    throw std::system_error(SocketError(), errorMessage);
  case SSL_ERROR_ZERO_RETURN:
    throw std::runtime_error(errorMessage);
  default:
    assert(false);
    return true;
  }
}

template<typename Deadline>
size_t SocketTlsImpl::SendPending(Deadline &deadline)
{
  size_t pending;
  while((pending = BIO_ctrl_pending(wbio)) || BIO_should_retry(wbio)) {
    //assert((pending > 0) && (buffer.size() >= static_cast<size_t>(pending)));
    buffer.resize(std::max(buffer.size(), static_cast<size_t>(pending)));
    auto received = BIO_read(wbio, buffer.data(), static_cast<int>(buffer.size()));
    if(received >= 0) {
      return sockpuppet::SendSome(this->fd, buffer.data(), static_cast<size_t>(received), deadline);
    }
  }
  return 0U;
}

template<typename Deadline>
bool SocketTlsImpl::ReceiveIncoming(Deadline &deadline)
{
  if(!WaitReadable(deadline.Remaining())) {
    return false; // timeout exceeded
  }
  if(auto received = sockpuppet::Receive(this->fd, buffer.data(), buffer.size())) {
    auto res = BIO_write(rbio, buffer.data(), static_cast<int>(received));
    assert(res == static_cast<int>(received));
  }
  return true;
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

std::pair<SocketTcp, Address> AcceptorTlsImpl::Accept()
{
  auto [client, addr] = SocketImpl::Accept();

  auto clientTls = std::make_unique<SocketTlsImpl>(
      std::move(*client.impl),
      ctx.get());

  SSL_set_accept_state(clientTls->ssl.get());

  return {SocketTcp(std::move(clientTls)), std::move(addr)};
}

} // namespace sockpuppet

#endif // SOCKPUPPET_WITH_TLS
