#include "socket.h"
#include "socket_address_priv.h" // for SocketAddress::SocketAddressPriv
#include "socket_priv.h" // for Socket::SocketPriv

#ifdef _WIN32
# include <Winsock2.h> // for IPPROTO_UDP
#else
# include <arpa/inet.h> // for IPPROTO_UDP
#endif // _WIN32

Socket::Socket(Socket &&other)
  : m_priv(std::move(other.m_priv))
{
}

Socket &Socket::operator=(Socket &&other)
{
  m_priv = std::move(other.m_priv);
  return *this;
}

Socket::Socket(std::unique_ptr<SocketPriv> &&other)
  : m_priv(std::move(other))
{
}

Socket::~Socket()
{
}

size_t Socket::GetReceiveBufferSize()
{
  auto const size = m_priv->GetSockOptRcvBuf();
  if(size < 0) {
    throw std::logic_error("invalid receive buffer size");
  }
  return static_cast<size_t>(size);
}


SocketUdp::SocketUdp(SocketAddress const &bindAddress)
  : Socket(std::make_unique<Socket::SocketPriv>(
      bindAddress.Priv()->Family(), SOCK_DGRAM, IPPROTO_UDP))
{
  m_priv->Bind(bindAddress.Priv()->SockAddrUdp());
  m_priv->SetSockOptBroadcast();
}

SocketUdp::SocketUdp(SocketUdp &&other)
  : Socket(std::move(other))
{
}

void SocketUdp::SendTo(char const *data, size_t size,
  SocketAddress const &dstAddress)
{
  m_priv->SendTo(data, size, dstAddress.Priv()->SockAddrUdp());
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


SocketTcpClient::SocketTcpClient(SocketAddress const &connectAddress)
  : Socket(std::make_unique<Socket::SocketPriv>(
      connectAddress.Priv()->Family(), SOCK_STREAM, IPPROTO_TCP))
{
  m_priv->Connect(connectAddress.Priv()->SockAddrTcp());
}

SocketTcpClient::SocketTcpClient(SocketTcpClient &&other)
  : Socket(std::move(other))
{
}

void SocketTcpClient::Send(const char *data, size_t size, Time timeout)
{
  m_priv->Send(data, size, timeout);
}

size_t SocketTcpClient::Receive(char *data, size_t size, Time timeout)
{
  return m_priv->Receive(data, size, timeout);
}

SocketTcpClient::SocketTcpClient(std::unique_ptr<Socket::SocketPriv> &&other)
  : Socket(std::move(other))
{
}


SocketTcpServer::SocketTcpServer(const SocketAddress &bindAddress)
  : Socket(std::make_unique<Socket::SocketPriv>(
      bindAddress.Priv()->Family(), SOCK_STREAM, IPPROTO_TCP))
{
  m_priv->SetSockOptReuseAddr();
  m_priv->Bind(bindAddress.Priv()->SockAddrTcp());
}

std::tuple<SocketTcpClient, SocketAddress> SocketTcpServer::Listen(Time timeout)
{
  auto t = m_priv->Listen(timeout);
  return std::tuple<SocketTcpClient, SocketAddress>{
    SocketTcpClient(std::move(std::get<0>(t)))
  , SocketAddress(std::move(std::get<1>(t)))
  };
}
