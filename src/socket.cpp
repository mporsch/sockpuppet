#include "socket.h"
#include "socket_address_priv.h" // for SocketAddress::SocketAddressPriv
#include "socket_priv.h" // for Socket::SocketPriv

#ifdef _WIN32
# include <Winsock2.h> // for IPPROTO_UDP
#else
# include <arpa/inet.h> // for IPPROTO_UDP
#endif // _WIN32

namespace sockpuppet {

SocketAddress Socket::LocalAddress() const
{
  return SocketAddress(m_priv->GetSockName());
}

size_t Socket::ReceiveBufferSize() const
{
  auto const size = m_priv->GetSockOptRcvBuf();
  if(size < 0) {
    throw std::logic_error("invalid receive buffer size");
  }
  return static_cast<size_t>(size);
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


SocketUdp::SocketUdp(SocketAddress const &bindAddress)
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

void SocketUdp::SendTo(char const *data, size_t size,
    SocketAddress const &dstAddress)
{
  m_priv->SendTo(data, size, dstAddress.Priv().ForUdp());
}

size_t SocketUdp::Receive(char *data, size_t size, Time timeout)
{
  return m_priv->Receive(data, size, timeout);
}

std::tuple<size_t, SocketAddress> SocketUdp::ReceiveFrom(
    char *data, size_t size, Time timeout)
{
  auto t = m_priv->ReceiveFrom(data, size, timeout);
  return std::tuple<size_t, SocketAddress>{
    std::get<0>(t)
  , SocketAddress(std::move(std::get<1>(t)))
  };
}

SocketAddress SocketUdp::BroadcastAddress(uint16_t port) const
{
  auto const localAddress = LocalAddress();

  if(localAddress.IsV6()) {
    throw std::invalid_argument("there are no IPv6 broadcast addresses");
  }

  return localAddress.Priv().ToBroadcast(port);
}


SocketTcpClient::SocketTcpClient(SocketAddress const &connectAddress)
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

void SocketTcpClient::Send(const char *data, size_t size, Time timeout)
{
  m_priv->Send(data, size, timeout);
}

size_t SocketTcpClient::Receive(char *data, size_t size, Time timeout)
{
  return m_priv->Receive(data, size, timeout);
}

SocketAddress SocketTcpClient::PeerAddress() const
{
  return SocketAddress(m_priv->GetPeerName());
}


SocketTcpServer::SocketTcpServer(const SocketAddress &bindAddress)
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

std::tuple<SocketTcpClient, SocketAddress> SocketTcpServer::Listen(Time timeout)
{
  m_priv->Listen();
  auto t = m_priv->Accept(timeout);
  return std::tuple<SocketTcpClient, SocketAddress>{
    SocketTcpClient(std::move(std::get<0>(t)))
  , SocketAddress(std::move(std::get<1>(t)))
  };
}

} // namespace sockpuppet
