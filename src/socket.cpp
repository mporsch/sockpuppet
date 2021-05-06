#include "sockpuppet/socket.h"
#include "address_priv.h" // for Address::AddressPriv
#include "socket_priv.h" // for SocketPriv
#include "socket_tls_priv.h" // for SocketTlsPriv

#ifdef _WIN32
# include <winsock2.h> // for IPPROTO_UDP
#else
# include <arpa/inet.h> // for IPPROTO_UDP
#endif // _WIN32

namespace sockpuppet {

SocketUdp::SocketUdp(Address const &bindAddress)
  : priv(std::make_unique<SocketPriv>(
      bindAddress.priv->Family(), SOCK_DGRAM, IPPROTO_UDP))
{
  priv->Bind(bindAddress.priv->ForUdp());
  priv->SetSockOptBroadcast();
}

size_t SocketUdp::SendTo(char const *data, size_t size,
    Address const &dstAddress, Duration timeout)
{
  return priv->SendTo(data, size, dstAddress.priv->ForUdp(), timeout);
}

std::optional<std::pair<size_t, Address>>
SocketUdp::ReceiveFrom(char *data, size_t size, Duration timeout)
{
  return priv->ReceiveFrom(data, size, timeout);
}

Address SocketUdp::LocalAddress() const
{
  return Address(priv->GetSockName());
}

size_t SocketUdp::ReceiveBufferSize() const
{
  return priv->GetSockOptRcvBuf();
}

SocketUdp::SocketUdp(SocketUdp &&other) noexcept = default;

SocketUdp::~SocketUdp() = default;

SocketUdp &SocketUdp::operator=(SocketUdp &&other) noexcept = default;


SocketTcpClient::SocketTcpClient(Address const &connectAddress)
  : priv(std::make_unique<SocketPriv>(
      connectAddress.priv->Family(), SOCK_STREAM, IPPROTO_TCP))
{
  priv->SetSockOptNoSigPipe();
  priv->Connect(connectAddress.priv->ForTcp());
}

SocketTcpClient::SocketTcpClient(Address const &connectAddress,
    char const *certFilePath, char const *keyFilePath)
  : priv(std::make_unique<SocketTlsPriv>(
      connectAddress.priv->Family(), SOCK_STREAM, IPPROTO_TCP,
      TLS_client_method(), certFilePath, keyFilePath))
{
  priv->SetSockOptNoSigPipe();
  priv->Connect(connectAddress.priv->ForTcp());
}

size_t SocketTcpClient::Send(char const *data, size_t size, Duration timeout)
{
  return priv->Send(data, size, timeout);
}

std::optional<size_t> SocketTcpClient::Receive(char *data, size_t size, Duration timeout)
{
  return priv->Receive(data, size, timeout);
}

Address SocketTcpClient::LocalAddress() const
{
  return Address(priv->GetSockName());
}

Address SocketTcpClient::PeerAddress() const
{
  return Address(priv->GetPeerName());
}

size_t SocketTcpClient::ReceiveBufferSize() const
{
  return priv->GetSockOptRcvBuf();
}

SocketTcpClient::SocketTcpClient(std::unique_ptr<SocketPriv> &&other)
  : priv(std::move(other))
{
  priv->SetSockOptNoSigPipe();
}

SocketTcpClient::SocketTcpClient(SocketTcpClient &&other) noexcept = default;

SocketTcpClient::~SocketTcpClient() = default;

SocketTcpClient &SocketTcpClient::operator=(SocketTcpClient &&other) noexcept = default;


SocketTcpServer::SocketTcpServer(Address const &bindAddress)
  : priv(std::make_unique<SocketPriv>(
      bindAddress.priv->Family(), SOCK_STREAM, IPPROTO_TCP))
{
  priv->SetSockOptReuseAddr();
  priv->Bind(bindAddress.priv->ForTcp());
}

SocketTcpServer::SocketTcpServer(Address const &bindAddress,
    char const *certFilePath, char const *keyFilePath)
  : priv(std::make_unique<SocketTlsPriv>(
      bindAddress.priv->Family(), SOCK_STREAM, IPPROTO_TCP,
      TLS_server_method(), certFilePath, keyFilePath))
{
  priv->SetSockOptReuseAddr();
  priv->Bind(bindAddress.priv->ForTcp());
}

std::optional<std::pair<SocketTcpClient, Address>>
SocketTcpServer::Listen(Duration timeout)
{
  priv->Listen();
  return priv->Accept(timeout);
}

Address SocketTcpServer::LocalAddress() const
{
  return Address(priv->GetSockName());
}

SocketTcpServer::SocketTcpServer(SocketTcpServer &&other) noexcept = default;

SocketTcpServer::~SocketTcpServer() = default;

SocketTcpServer &SocketTcpServer::operator=(SocketTcpServer &&other) noexcept = default;

} // namespace sockpuppet
