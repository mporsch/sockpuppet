#ifdef WITH_TLS

#include "socket_tls_priv.h"
#include "error_code.h" // for SocketError

namespace sockpuppet {

namespace {

std::shared_ptr<SSL_CTX> create_ssl_ctx(
    std::string_view cert, std::string_view key,
    const SSL_METHOD* method)
{
    auto ctx = std::shared_ptr<SSL_CTX>(
          SSL_CTX_new(method),
          [](SSL_CTX* ctx) {
            if(ctx) {
                SSL_CTX_free(ctx);
            }
          });
    if(!ctx) {
        throw std::runtime_error("Failed to create TLS context");
    }
    SSL_CTX_set_mode(ctx.get(), SSL_MODE_AUTO_RETRY);
    SSL_CTX_set_ecdh_auto(ctx.get(), 1);

    if(SSL_CTX_use_certificate_file(ctx.get(), cert.data(), SSL_FILETYPE_PEM) <= 0) {
        throw std::runtime_error("Failed to set certificate");
    }
    if(SSL_CTX_use_PrivateKey_file(ctx.get(), key.data(), SSL_FILETYPE_PEM) <= 0) {
        throw std::runtime_error("Failed to set private key");
    }
    return ctx;
}

} // unnamed namespace

SocketTlsPriv::SocketTlsPriv(int family, int type, int protocol,
    const SSL_METHOD* method, std::string_view cert_path, std::string_view key_path)
  : SocketPriv(family, type, protocol)
  , sslGuard()
  , ctx(create_ssl_ctx(cert_path, key_path, method))
  , ssl(SSL_new(ctx.get()))
{
  if(!ssl) {
      throw std::runtime_error("Failed to instatiate SSL structure");
  }
  SSL_set_fd(ssl, this->fd);
}

SocketTlsPriv::SocketTlsPriv(SOCKET fd, std::shared_ptr<SSL_CTX> ctx)
  : SocketPriv(fd)
  , sslGuard()
  , ctx(std::move(ctx))
  , ssl(SSL_new(this->ctx.get()))
{
  if(!ssl) {
      throw std::runtime_error("Failed to instatiate SSL structure");
  }
  SSL_set_fd(ssl, this->fd);
}

SocketTlsPriv::~SocketTlsPriv()
{
  if(ssl) {
      SSL_shutdown(ssl);
      SSL_free(ssl);
  }
}

size_t SocketTlsPriv::Receive(char *data, size_t size)
{
  auto const res = SSL_read(ssl, data, size);
  if(res <= 0) {
    throw std::system_error(SslError(SSL_get_error(ssl, res)), "failed to TLS receive");
  }
  return static_cast<size_t>(res);
}

size_t SocketTlsPriv::SendAll(char const *data, size_t size)
{
  auto const res = SSL_write(ssl, data, size);
  if(res <= 0) {
    throw std::system_error(SslError(SSL_get_error(ssl, res)), "failed to TLS send");
  }
  return static_cast<size_t>(res);
}

size_t SocketTlsPriv::SendSome(char const *data, size_t size)
{
  auto const res = SSL_write(ssl, data, size);
  if(res <= 0) {
    throw std::system_error(SslError(SSL_get_error(ssl, res)), "failed to TLS send");
  }
  return static_cast<size_t>(res);
}

void SocketTlsPriv::Connect(SockAddrView const &connectAddr)
{
  SocketPriv::Connect(connectAddr);

  auto res = SSL_connect(ssl);
  if(res != 1) {
      throw std::system_error(SslError(SSL_get_error(ssl, res)), "Failed to TLS connect");
  }
}

std::pair<SocketTcpClient, Address>
SocketTlsPriv::Accept()
{
  auto sas = std::make_shared<SockAddrStorage>();
  auto clientTls = std::make_unique<SocketTlsPriv>(
        ::accept(fd, sas->Addr(), sas->AddrLen()),
        ctx);

  auto res = SSL_accept(clientTls->ssl);
  if(res != 1) {
    throw std::system_error(SslError(SSL_get_error(ssl, res)), "Failed to TLS accept");
  }

  std::unique_ptr<SocketPriv> client = std::move(clientTls);
  return {std::move(client), Address(std::move(sas))};
}

} // namespace sockpuppet

#endif // WITH_TLS
