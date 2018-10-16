#include "socket.h"
#include "socket_address_priv.h" // for SocketAddress::SocketAddressPriv
#include "socket_priv.h" // for Socket::SocketPriv

#ifdef _WIN32
# include <winsock2.h> // for IPPROTO_UDP
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


SocketUdp::SocketUdp(SocketAddress const &bindAddress)
  : Socket(std::make_unique<Socket::SocketPriv>(
      bindAddress.priv->Family(), SOCK_DGRAM, IPPROTO_UDP))
{
  m_priv->Bind(bindAddress.priv->SockAddrUdp());
}

void SocketUdp::SendTo(char const *data, size_t size,
  SocketAddress const &dstAddress)
{
  m_priv->SendTo(data, size, dstAddress.priv->SockAddrUdp());
}

size_t SocketUdp::Receive(char *data, size_t size,
  Socket::Time timeout)
{
  return m_priv->Receive(data, size, timeout);
}

std::tuple<size_t, SocketAddress> SocketUdp::ReceiveFrom(
  char *data, size_t size, Time timeout)
{
  auto t = m_priv->ReceiveFrom(data, size, timeout);
  return {
    std::get<0>(t)
  , SocketAddress(std::move(std::get<1>(t)))
  };
}


SocketTcpClient::SocketTcpClient(SocketAddress const &connectAddress)
  : Socket(std::make_unique<Socket::SocketPriv>(
      connectAddress.priv->Family(), SOCK_STREAM, IPPROTO_TCP))
{
  m_priv->Connect(connectAddress.priv->SockAddrTcp());
}

void SocketTcpClient::Send(const char *data, size_t size,
  Time timeout)
{
  m_priv->Send(data, size, timeout);
}

size_t SocketTcpClient::Receive(char *data, size_t size,
  Socket::Time timeout)
{
  return m_priv->Receive(data, size, timeout);
}

SocketTcpClient::SocketTcpClient(std::unique_ptr<Socket::SocketPriv> &&other)
  : Socket(std::move(other))
{
}


SocketTcpServer::SocketTcpServer(const SocketAddress &bindAddress)
  : Socket(std::make_unique<Socket::SocketPriv>(
      bindAddress.priv->Family(), SOCK_STREAM, IPPROTO_TCP))
{
  m_priv->SetSockOptReuseAddr();
  m_priv->Bind(bindAddress.priv->SockAddrTcp());
}

std::tuple<SocketTcpClient, SocketAddress> SocketTcpServer::Listen(Time timeout)
{
  auto t = m_priv->Listen(timeout);
  return {
    SocketTcpClient(std::move(std::get<0>(t)))
  , SocketAddress(std::move(std::get<1>(t)))
  };
}
