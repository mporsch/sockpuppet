#ifdef SOCKPUPPET_WITH_TLS

#include "socket_tls_impl.h"
#include "error_code.h" // for SslError

#include <openssl/bio.h> // for BIO

#include <cassert> // for assert
#include <stdexcept> // for std::logic_error
#include <type_traits> // for std::is_same_v

namespace sockpuppet {

namespace {

// loop over OpenSSL calls that might perform a handshake at any time
// the number is arbitrary and used only to avoid/detect infinite loops
constexpr int handshakeStepsMax = 10;

constexpr auto zeroTimeout = Duration(0);

namespace bio {

int Write(BIO *b, char const *data, int s)
{
  auto *sock = reinterpret_cast<SocketTlsImpl *>(BIO_get_data(b));
  auto size = static_cast<size_t>(s);

  auto sent = std::visit([=](auto &&deadline) -> size_t {
    using Deadline = std::decay_t<decltype(deadline)>;
    if constexpr(std::is_same_v<Deadline, DeadlineUnlimited>) {
      return SendAll(sock->fd, data, size);
    } else {
      return SendSome(sock->fd, data, size, deadline);
    }
  }, sock->deadline);

  if(sent > 0U) {
    BIO_clear_retry_flags(b);
  } else {
    BIO_set_retry_write(b);
  }

  return static_cast<int>(sent);
}

int Read(BIO *b, char *data, int s)
{
  auto *sock = reinterpret_cast<SocketTlsImpl *>(BIO_get_data(b));
  auto size = static_cast<size_t>(s);

  auto received = sock->UnderDeadline([=](Duration timeout) -> std::optional<size_t> {
    return Receive(sock->fd, data, size, timeout);
  });

  if(received) {
    BIO_clear_retry_flags(b);
    return static_cast<int>(*received);
  }
  BIO_set_retry_read(b);
  return 0;
}

long Ctrl(BIO *, int cmd, long, void *)
{
  switch(cmd) {
  case BIO_CTRL_FLUSH:
    return 1;
  default:
    return 0;
  }
}

int Create(BIO *b)
{
  BIO_set_init(b, 1);
  return 1;
}

} // namespace bio

struct BioDeleter
{
  void operator()(BIO *ptr) const noexcept
  {
    (void)BIO_free(ptr);
  }
};
using BioPtr = std::unique_ptr<BIO, BioDeleter>;

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
    if(BIO_meth_set_write(method.get(), bio::Write) &&
       BIO_meth_set_read(method.get(), bio::Read) &&
       BIO_meth_set_ctrl(method.get(), bio::Ctrl) &&
       BIO_meth_set_create(method.get(), bio::Create)) {
      return method;
    }
    throw std::logic_error("failed to create BIO method");
  }();
  return instance.get();
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
  throw std::logic_error("failed to create SSL context");
}

void ConfigureSsl(SSL *ssl, SocketTlsImpl *sock)
{
  auto rbio = BioPtr(BIO_new(BIO_s_sockpuppet()));
  auto wbio = BioPtr(BIO_new(BIO_s_sockpuppet()));
  if(rbio && wbio) {
    BIO_set_data(rbio.get(), sock);
    BIO_set_data(wbio.get(), sock);
    SSL_set_bio(ssl, rbio.release(), wbio.release());
  } else {
    throw std::logic_error("failed to create read/write BIO");
  }
}

SocketTlsImpl::SslPtr CreateSsl(SSL_CTX *ctx, SocketTlsImpl *sock)
{
  if(auto ssl = SocketTlsImpl::SslPtr(SSL_new(ctx))) {
    ConfigureSsl(ssl.get(), sock);
    return ssl;
  }
  throw std::logic_error("failed to create SSL structure");
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
  , ssl(CreateSsl( // context is reference-counted by itself -> free temporary handle
          CreateCtx(TLS_client_method(), certFilePath, keyFilePath).get(),
          this))
{
}

SocketTlsImpl::SocketTlsImpl(SocketImpl &&sock, SSL_CTX *ctx)
  : SocketImpl(std::move(sock))
  , sslGuard()
  , ssl(CreateSsl(ctx, this))
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
  if(auto received = Read(data, size, timeout)) {
    return {received};
  }

  // unlimited timeout performs full handshake and subsequent receive
  assert(timeout.count() >= 0);
  return {std::nullopt};
}

size_t SocketTlsImpl::Receive(char *data, size_t size)
{
  // we have been deemed readable
  if(lastError == SSL_ERROR_WANT_READ) {
    lastError = SSL_ERROR_NONE;
  }

  return Read(data, size, zeroTimeout);
}

size_t SocketTlsImpl::Send(char const *data, size_t size,
    Duration timeout)
{
  return Write(data, size, timeout);
}

size_t SocketTlsImpl::SendSome(char const *data, size_t size)
{
  // we have been deemed writable/readable
  if(lastError == SSL_ERROR_WANT_WRITE ||
     (pendingSend && lastError == SSL_ERROR_WANT_READ)) {
    lastError = SSL_ERROR_NONE;
  }

  return Write(data, size, zeroTimeout);
}

void SocketTlsImpl::Connect(SockAddrView const &connectAddr)
{
  SocketImpl::Connect(connectAddr);

  SSL_set_connect_state(ssl.get());
}

size_t SocketTlsImpl::Read(char *data, size_t size, Duration timeout)
{
  SetDeadline(timeout);

  if(HandleLastError()) {
    for(int i = 1; i <= handshakeStepsMax; ++i) {
      auto res = SSL_read(ssl.get(), data, static_cast<int>(size));
      if(res < 0) {
        if(!HandleResult(res)) {
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

size_t SocketTlsImpl::Write(char const *data, size_t size, Duration timeout)
{
  SetDeadline(timeout);

  if(pendingSend) {
    assert(pendingSend == data);
    pendingSend = nullptr;
  }

  size_t sent = 0U;
  if(HandleLastError()) {
    for(int i = 1; (i <= handshakeStepsMax) && (sent < size); ++i) {
      size_t written = 0U;
      auto res = SSL_write_ex(ssl.get(), data + sent, size - sent, &written);
      if(res <= 0) {
        if(!HandleResult(res)) {
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

void SocketTlsImpl::Shutdown()
{
  SetDeadline(std::chrono::seconds(1));

  if(SSL_shutdown(ssl.get()) <= 0) {
    // sent the shutdown, but have not received one from the peer yet
    // spend some time trying to receive it, but go on eventually
    char buf[1024];
    for(int i = 0; i < handshakeStepsMax; ++i) {
      auto res = SSL_read(ssl.get(), buf, sizeof(buf));
      if(res < 0) {
        if(!HandleResult(res)) {
          break;
        }
      } else if(res == 0) {
        break;
      }
    }

    (void)SSL_shutdown(ssl.get());
  }
}

void SocketTlsImpl::SetDeadline(Duration timeout)
{
  if(timeout.count() < 0) {
    deadline.emplace<DeadlineUnlimited>();
  } else if(timeout.count() == 0) {
    deadline.emplace<DeadlineZero>();
  } else {
    deadline.emplace<DeadlineLimited>(timeout);
  }
}

bool SocketTlsImpl::HandleResult(int res)
{
  auto error = SSL_get_error(ssl.get(), res);
  if(HandleError(error)) {
    lastError = SSL_ERROR_NONE;
    return true;
  }
  lastError = error;
  return false;
}

bool SocketTlsImpl::HandleLastError()
{
  if(HandleError(lastError)) {
    lastError = SSL_ERROR_NONE;
    return true;
  }
  return false;
}

bool SocketTlsImpl::HandleError(int error)
{
  constexpr char errorMessage[] = "failed to TLS receive/send/handshake";

  switch(error) {
  case SSL_ERROR_NONE:
    return true;
  case SSL_ERROR_WANT_READ:
    return UnderDeadline([this](Duration timeout) -> bool {
      return WaitReadableBlocking(this->fd, timeout);
    });
  case SSL_ERROR_WANT_WRITE:
    return UnderDeadline([this](Duration timeout) -> bool {
      return WaitWritableBlocking(this->fd, timeout);
    });
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
