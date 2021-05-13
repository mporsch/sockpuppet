#ifdef WITH_TLS

#include "socket_tls_priv.h"
#include "error_code.h" // for SocketError

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

size_t SocketTlsClientPriv::Receive(char *data, size_t size)
{
  auto const res = SSL_read(ssl.get(), data, size);
  if(res < 0) {
    throw MakeSslError(ssl.get(), res, "failed to TLS receive");
  } else if(res == 0) {
    throw std::runtime_error("connection closed");
  }
  return static_cast<size_t>(res);
}

size_t SocketTlsClientPriv::SendAll(char const *data, size_t size)
{
  auto const res = SSL_write(ssl.get(), data, size);
  if(res < 0) {
    throw MakeSslError(ssl.get(), res, "failed to TLS send");
  } else if((res == 0) && (size > 0U)) {
    throw std::logic_error("unexpected send result");
  }
  return static_cast<size_t>(res);
}

size_t SocketTlsClientPriv::SendSome(char const *data, size_t size)
{
  auto const res = SSL_write(ssl.get(), data, size);
  if(res < 0) {
    throw MakeSslError(ssl.get(), res, "failed to TLS send");
  } else if((res == 0) && (size > 0U)) {
    throw std::logic_error("unexpected send result");
  }
  return static_cast<size_t>(res);
}

void SocketTlsClientPriv::Connect(SockAddrView const &connectAddr)
{
  SocketPriv::Connect(connectAddr);

  auto res = SSL_connect(ssl.get());
  if(res != 1) {
      throw MakeSslError(ssl.get(), res, "failed to TLS connect");
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

  auto clientTls = std::make_unique<SocketTlsClientPriv>(
      std::move(*client),
      ctx.get());

  auto res = SSL_accept(clientTls->ssl.get());
  if(res != 1) {
    throw MakeSslError(clientTls->ssl.get(), res, "failed to TLS accept");
  }

  return {std::move(clientTls), std::move(addr)};
}

} // namespace sockpuppet

#endif // WITH_TLS
