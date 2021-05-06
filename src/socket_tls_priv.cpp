#ifdef WITH_TLS

#include "socket_tls_priv.h"
#include "error_code.h" // for SocketError

namespace sockpuppet {

namespace {

// context is reference-counted by itself: used for transport to session only
using CtxPtr = std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)>;

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

CtxPtr CreateContext(SSL_METHOD const *method,
    char const *certFilePath, char const *keyFilePath)
{
  if(auto ctx = CtxPtr(SSL_CTX_new(method), SSL_CTX_free)) {
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

SocketTlsPriv::SslPtr CreateSsl(SSL_CTX *ctx, SOCKET fd)
{
  if(auto ssl = SocketTlsPriv::SslPtr(SSL_new(ctx), FreeSsl)) {
    SSL_set_fd(ssl.get(), fd);
    return ssl;
  }
  throw std::runtime_error("failed to create SSL structure");
}

} // unnamed namespace

SocketTlsPriv::SocketTlsPriv(int family, int type, int protocol,
    SSL_METHOD const *method, char const *certFilePath, char const *keyFilePath)
  : SocketPriv(family, type, protocol)
  , sslGuard()
  , ssl(CreateSsl(CreateContext(method, certFilePath, keyFilePath).get(), this->fd))
{
}

SocketTlsPriv::SocketTlsPriv(SOCKET fd, SSL_CTX *ctx)
  : SocketPriv(fd)
  , sslGuard()
  , ssl(CreateSsl(ctx, this->fd))
{
}

SocketTlsPriv::~SocketTlsPriv() = default;

size_t SocketTlsPriv::Receive(char *data, size_t size)
{
  auto const res = SSL_read(ssl.get(), data, size);
  if(res < 0) {
    throw std::system_error(SslError(SSL_get_error(ssl.get(), res)), "failed to TLS receive");
  } else if(res == 0) {
    throw std::runtime_error("connection closed");
  }
  return static_cast<size_t>(res);
}

size_t SocketTlsPriv::SendAll(char const *data, size_t size)
{
  auto const res = SSL_write(ssl.get(), data, size);
  if(res < 0) {
    throw std::system_error(SslError(SSL_get_error(ssl.get(), res)), "failed to TLS send");
  } else if((res == 0) && (size > 0U)) {
    throw std::logic_error("unexpected send result");
  }
  return static_cast<size_t>(res);
}

size_t SocketTlsPriv::SendSome(char const *data, size_t size)
{
  auto const res = SSL_write(ssl.get(), data, size);
  if(res < 0) {
    throw std::system_error(SslError(SSL_get_error(ssl.get(), res)), "failed to TLS send");
  } else if((res == 0) && (size > 0U)) {
    throw std::logic_error("unexpected send result");
  }
  return static_cast<size_t>(res);
}

void SocketTlsPriv::Connect(SockAddrView const &connectAddr)
{
  SocketPriv::Connect(connectAddr);

  auto res = SSL_connect(ssl.get());
  if(res != 1) {
      throw std::system_error(SslError(SSL_get_error(ssl.get(), res)), "failed to TLS connect");
  }
}

std::pair<SocketTcpClient, Address>
SocketTlsPriv::Accept()
{
  auto sas = std::make_shared<SockAddrStorage>();
  auto client = std::make_unique<SocketTlsPriv>(
        ::accept(fd, sas->Addr(), sas->AddrLen()),
        ssl->ctx);

  auto res = SSL_accept(client->ssl.get());
  if(res != 1) {
    throw std::system_error(SslError(SSL_get_error(ssl.get(), res)), "failed to TLS accept");
  }

  return {std::unique_ptr<SocketPriv>(std::move(client)), Address(std::move(sas))};
}

} // namespace sockpuppet

#endif // WITH_TLS
