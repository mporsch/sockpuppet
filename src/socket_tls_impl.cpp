#ifdef SOCKPUPPET_WITH_TLS

#include "socket_tls_impl.h"
#include "error_code.h" // for SslError
#include "wait.h" // for DeadlineLimited

#include <openssl/bio.h> // for BIO

#include <cassert> // for assert
#include <stdexcept> // for std::logic_error

namespace sockpuppet {

namespace {

constexpr auto zeroTimeout = Duration(0);

// loop over OpenSSL calls that might perform a handshake at any time
// the number is arbitrary and used only to avoid/detect infinite loops
constexpr int handshakeStepsMax = 10;

template<typename Fn>
auto UnderDeadline(Fn &&fn, Duration &timeout) -> auto
{
  if(timeout.count() <= 0) { // remains unchanged
    return fn();
  }

  // update timeout (may be exceeded)
  DeadlineLimited deadline(timeout);
  auto res = fn();
  deadline.Tick();
  timeout = deadline.Remaining();
  return res;
}

// providing our own OpenSSL BIO implementation allows us to
// tunnel calls through SSL_write/SSL_read/SSL_shutdown back to
// our own socket implementation that honors the given timeout value
// BIO_set_data/BIO_get_data connect the socket to the BIO implementation
namespace bio {

int Read(BIO *b, char *data, int size)
{
  auto *sock = reinterpret_cast<SocketTlsImpl *>(BIO_get_data(b));

  BIO_clear_retry_flags(b);

  auto received = sock->BioRead(data, static_cast<size_t>(size));
  if(!received) {
    BIO_set_retry_read(b);
  }

  return static_cast<int>(received);
}

int Write(BIO *b, char const *data, int size)
{
  auto *sock = reinterpret_cast<SocketTlsImpl *>(BIO_get_data(b));

  BIO_clear_retry_flags(b);

  auto sent = sock->BioWrite(data, static_cast<size_t>(size));
  if(sent != static_cast<size_t>(size)) {
    BIO_set_retry_write(b);
  }

  return static_cast<int>(sent);
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

// since we don't allocate any memory or store any state, the "create" method
// does very little and we don't even need to provide a "destroy" method
int Create(BIO *b)
{
  BIO_set_init(b, 1);
  return 1;
}

struct MethodDeleter
{
  void operator()(BIO_METHOD *ptr) const noexcept
  {
    BIO_meth_free(ptr);
  }
};
using MethodPtr = std::unique_ptr<BIO_METHOD, MethodDeleter>;

// the "recipe" that OpenSSL uses to create our custom BIO
MethodPtr CreateMethod()
{
  auto method = MethodPtr(BIO_meth_new(
      BIO_get_new_index() | BIO_TYPE_SOURCE_SINK,
      "sockpuppet"));
  if(BIO_meth_set_read(method.get(), Read) &&
     BIO_meth_set_write(method.get(), Write) &&
     BIO_meth_set_ctrl(method.get(), Ctrl) &&
     BIO_meth_set_create(method.get(), Create)) {
    return method;
  }
  throw std::logic_error("failed to create BIO method");
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

// follow OpenSSL naming scheme as in BIO_s_mem, BIO_s_socket, ...
BIO_METHOD *BIO_s_sockpuppet()
{
  // singleton instance that is kept until program exit
  static auto instance = bio::CreateMethod();
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
  if(!rbio || !wbio) {
    throw std::logic_error("failed to create read/write BIO");
  }

  BIO_set_data(rbio.get(), sock);
  BIO_set_data(wbio.get(), sock);
  SSL_set_bio(ssl, rbio.release(), wbio.release()); // SSL takes ownership of BIOs
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

SocketTlsImpl::SocketTlsImpl(SOCKET fd, SSL_CTX *ctx)
  : SocketImpl(fd)
  , sslGuard()
  , ssl(CreateSsl(ctx, this))
{
}

SocketTlsImpl::~SocketTlsImpl()
{
  switch(lastError) {
  case SSL_ERROR_SYSCALL:
  case SSL_ERROR_SSL:
    break; // don't attempt clean shutdown after fatal errors
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
  // timeout will be honored during waiting and BIO read/write
  remainingTime = timeout;

  if(auto received = Read(data, size)) {
    return {received};
  }

  assert(timeout.count() >= 0); // unlimited timeout performs full handshake and waits for receive
  return {std::nullopt};
}

size_t SocketTlsImpl::Receive(char *data, size_t size)
{
  // we have been deemed readable
  remainingTime = zeroTimeout;
  isReadable = true;
  if(lastError == SSL_ERROR_WANT_READ) {
    lastError = SSL_ERROR_NONE;
  }

  auto received = Read(data, size);
  if(!received && SSL_is_init_finished(ssl.get())) {
    // don't get stuck trying to read something after being readable during handshake
    lastError = SSL_ERROR_NONE;
  }
  return received;
}

size_t SocketTlsImpl::Send(char const *data, size_t size, Duration timeout)
{
  // timeout will be honored during waiting and BIO read/write
  remainingTime = timeout;

  return Write(data, size);
}

size_t SocketTlsImpl::SendSome(char const *data, size_t size)
{
  // we have been deemed writable
  remainingTime = zeroTimeout;
  isWritable = true;
  if(lastError == SSL_ERROR_WANT_WRITE) {
    lastError = SSL_ERROR_NONE;
  }

  return Write(data, size);
}

void SocketTlsImpl::Connect(SockAddrView const &connectAddr)
{
  SocketImpl::Connect(connectAddr);

  SSL_set_connect_state(ssl.get());
  // the TLS handshake will be performed during Send/Receive
}

void SocketTlsImpl::DriverQuery(short &events)
{
  if(!SSL_is_init_finished(ssl.get())) {
    if(lastError == SSL_ERROR_WANT_WRITE) {
      // while handshake send is pending we actively request send attempts from the Driver
      events |= POLLOUT;
    } else if(lastError == SSL_ERROR_WANT_READ) {
      // while handshake receive is pending we suppress send attempts from the Driver
      driverSendSuppressed |= (events & POLLOUT);
      events &= ~POLLOUT;
    }
  } else if(driverSendSuppressed) {
    // if Driver send has been suppressed before handshake finished, restore it now
    driverSendSuppressed = false;
    events |= POLLOUT;
  }
}

void SocketTlsImpl::DriverPending()
{
  if(SSL_is_init_finished(ssl.get())) {
    return;
  }

  // we have been deemed writable
  remainingTime = zeroTimeout;
  isWritable = true;
  if(lastError == SSL_ERROR_WANT_WRITE) {
    lastError = SSL_ERROR_NONE;
  }

  char buf[64];
  auto received = Read(buf, sizeof(buf));
  if(received) {
    throw std::logic_error("unexpected recceive");
  }
}

void SocketTlsImpl::Shutdown()
{
  // timeout will be honored during waiting and BIO read/write
  isReadable = false;
  isWritable = false;
  remainingTime = std::chrono::seconds(1);

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

size_t SocketTlsImpl::Read(char *data, size_t size)
{
  if(HandleLastError()) {
    for(int i = 1; i <= handshakeStepsMax; ++i) {
      auto res = SSL_read(ssl.get(), data, static_cast<int>(size));
      if(res <= 0) {
        if(!HandleResult(res)) {
          break;
        }
      } else {
        return static_cast<size_t>(res);
      }
      assert(i < handshakeStepsMax);
    }
  }
  return 0U; // timeout / socket was readable for TLS handshake only
}

size_t SocketTlsImpl::BioRead(char *data, size_t size)
{
  if(isReadable) {
    isReadable = false;
    return ReceiveNow(this->fd, data, size);
  }

  // (try to) receive handshake / user data and update remaining time
  auto received = UnderDeadline([=]() -> std::optional<size_t> {
    return sockpuppet::Receive(this->fd, data, size, remainingTime);
  }, remainingTime);
  if(received) {
    return *received;
  }
  return 0U;
}

size_t SocketTlsImpl::Write(char const *data, size_t size)
{
  auto remaining = std::string_view(data, size);

  if(HandleLastError()) {
    for(int i = 1; (i <= handshakeStepsMax) && !remaining.empty(); ++i) {
      // if a previous TLS send failed (because handshake receipt was pending or
      // TCP congestion control blocked) we must repeat the call with the same buffer
      // see https://www.openssl.org/docs/man1.1.1/man3/SSL_write.html
      assert(pendingSend.empty() || (pendingSend == remaining));

      size_t written = 0U;
      auto res = SSL_write_ex(ssl.get(), remaining.data(), remaining.size(), &written);
      if(res <= 0) {
        pendingSend = remaining;
        if(!HandleResult(res)) {
          break; // timeout / socket was writable for TLS handshake only
        }
      } else {
        pendingSend = {};
        assert(written <= remaining.size());
        remaining.remove_prefix(written);
      }
      assert(i < handshakeStepsMax);
    }
  }

  assert(size >= remaining.size());
  return size - remaining.size();
}

size_t SocketTlsImpl::BioWrite(char const *data, size_t size)
{
  if(isWritable) {
    isWritable = false;
    return SendNow(this->fd, data, size);
  }

  if(remainingTime.count() < 0) { // remains unchanged
    return SendAll(this->fd, data, size);
  }

  if(remainingTime.count() == 0) { // remains unchanged
    return SendTry(this->fd, data, size);
  }

  // (try to) send handshake / user data and update remaining time
  DeadlineLimited deadline(remainingTime);
  auto sent = sockpuppet::SendSome(this->fd, data, size, deadline);
  remainingTime = (sent == size ? deadline.Remaining() : zeroTimeout); // update timeout (may be exceeded)
  return sent;
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
    // wait and update remaining time
    return UnderDeadline([this]() -> bool {
      return WaitReadable(this->fd, remainingTime);
    }, remainingTime);
  case SSL_ERROR_WANT_WRITE:
    return UnderDeadline([this]() -> bool {
      return WaitWritable(this->fd, remainingTime);
    }, remainingTime);
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
  auto [clientFd, clientAddr] = sockpuppet::Accept(this->fd);
  auto clientSock = std::make_unique<SocketTlsImpl>(clientFd, ctx.get());

  SSL_set_accept_state(clientSock->ssl.get());
  // the TLS handshake will be performed during Send/Receive

  return {
    SocketTcp(std::move(clientSock)),
    std::move(clientAddr)
  };
}

} // namespace sockpuppet

#endif // SOCKPUPPET_WITH_TLS
