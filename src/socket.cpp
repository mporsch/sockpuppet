#include "sockpuppet/socket.h"
#include "address_priv.h" // for Address::AddressPriv
#include "socket_priv.h" // for Socket::SocketPriv

#ifdef _WIN32
# include <winsock2.h> // for IPPROTO_UDP
#else
# include <arpa/inet.h> // for IPPROTO_UDP
#endif // _WIN32

namespace sockpuppet {

Address Socket::LocalAddress() const
{
  return Address(m_priv->GetSockName());
}

size_t Socket::ReceiveBufferSize() const
{
  return m_priv->GetSockOptRcvBuf();
}

Socket::Socket(std::unique_ptr<SocketPriv> &&other)
  : m_priv(std::move(other))
{
}

Socket::Socket(Socket &&other) noexcept
  : m_priv(std::move(other.m_priv))
{
}

Socket::~Socket() = default;

Socket &Socket::operator=(Socket &&other) noexcept
{
  m_priv = std::move(other.m_priv);
  return *this;
}


SocketUdp::SocketUdp(Address const &bindAddress)
  : Socket(std::make_unique<Socket::SocketPriv>(
      bindAddress.Priv().Family(), SOCK_DGRAM, IPPROTO_UDP))
{
  m_priv->Bind(bindAddress.Priv().ForUdp());
  m_priv->SetSockOptBroadcast();
}

SocketUdp::SocketUdp(SocketUdp &&other) noexcept
  : Socket(std::move(other))
{
}

SocketUdp::~SocketUdp() = default;


SocketUdp &SocketUdp::operator=(SocketUdp &&other) noexcept
{
  Socket::operator=(std::move(other));
  return *this;
}

size_t SocketUdp::SendTo(char const *data, size_t size,
    Address const &dstAddress, Duration timeout)
{
  return m_priv->SendTo(data, size, dstAddress.Priv().ForUdp(), timeout);
}

size_t SocketUdp::Receive(char *data, size_t size, Duration timeout)
{
  return m_priv->Receive(data, size, timeout);
}

std::tuple<size_t, Address> SocketUdp::ReceiveFrom(
    char *data, size_t size, Duration timeout)
{
  auto t = m_priv->ReceiveFrom(data, size, timeout);
  return std::tuple<size_t, Address>{
    std::get<0>(t)
  , Address(std::move(std::get<1>(t)))
  };
}


SocketTcpClient::SocketTcpClient(Address const &connectAddress)
  : Socket(std::make_unique<Socket::SocketPriv>(
      connectAddress.Priv().Family(), SOCK_STREAM, IPPROTO_TCP))
{
  m_priv->Connect(connectAddress.Priv().ForTcp());
}

SocketTcpClient::SocketTcpClient(std::unique_ptr<Socket::SocketPriv> &&other)
  : Socket(std::move(other))
{
}

SocketTcpClient::SocketTcpClient(SocketTcpClient &&other) noexcept
  : Socket(std::move(other))
{
}

SocketTcpClient::~SocketTcpClient() = default;

SocketTcpClient &SocketTcpClient::operator=(SocketTcpClient &&other) noexcept
{
  Socket::operator=(std::move(other));
  return *this;
}

size_t SocketTcpClient::Send(const char *data, size_t size, Duration timeout)
{
  return m_priv->Send(data, size, timeout);
}

size_t SocketTcpClient::Receive(char *data, size_t size, Duration timeout)
{
  return m_priv->Receive(data, size, timeout);
}

Address SocketTcpClient::PeerAddress() const
{
  return Address(m_priv->GetPeerName());
}


SocketTcpServer::SocketTcpServer(const Address &bindAddress)
  : Socket(std::make_unique<Socket::SocketPriv>(
      bindAddress.Priv().Family(), SOCK_STREAM, IPPROTO_TCP))
{
  m_priv->SetSockOptReuseAddr();
  m_priv->Bind(bindAddress.Priv().ForTcp());
}

SocketTcpServer::SocketTcpServer(SocketTcpServer &&other) noexcept
  : Socket(std::move(other))
{
}

SocketTcpServer::~SocketTcpServer() = default;

SocketTcpServer &SocketTcpServer::operator=(SocketTcpServer &&other) noexcept
{
  Socket::operator=(std::move(other));
  return *this;
}

std::tuple<SocketTcpClient, Address> SocketTcpServer::Listen(Duration timeout)
{
  m_priv->Listen();
  auto t = m_priv->Accept(timeout);
  return std::tuple<SocketTcpClient, Address>{
    SocketTcpClient(std::move(std::get<0>(t)))
  , Address(std::move(std::get<1>(t)))
  };
}

} // namespace sockpuppet
