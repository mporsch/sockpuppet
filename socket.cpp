#include "socket.h"
#include "socket_address_priv.h" // for SocketAddress::SocketAddressPriv

#ifdef _WIN32
# include <winsock2.h> // for ::socket
# pragma comment(lib, "Ws2_32.lib")
#else
# include <arpa/inet.h> // for IPPROTO_UDP
# include <sys/socket.h> // for ::socket
# include <unistd.h> // for ::close
#endif // _WIN32

#include <stdexcept> // for std::runtime_error

Socket::~Socket()
{
  if(m_fd >= 0) {
#ifdef _WIN32
    (void)closesocket(m_fd);
#else
    (void)::close(m_fd);
#endif // _WIN32
  }
}

Socket::Socket(Socket &&other)
  : m_socketGuard(std::move(other.m_socketGuard))
  , m_fd(std::move(other.m_fd))
{
  other.m_fd = -1;
}

Socket &Socket::operator=(Socket &&other)
{
  m_socketGuard = std::move(other.m_socketGuard);
  m_fd = std::move(other.m_fd);
  other.m_fd = -1;
  return *this;
}

size_t Socket::Receive(char *data, size_t size)
{
  return ::recv(m_fd, data, size, 0);
}

Socket::Socket(int family, int type, int protocol)
  : m_fd(::socket(family, type, protocol))
{
  if(m_fd < 0) {
    throw std::runtime_error("failed to create socket: "
                             + std::to_string(errno));
  }
}

Socket::Socket(int fd)
  : m_fd(fd)
{
  if(m_fd < 0) {
    throw std::runtime_error("failed to create socket: "
                             + std::to_string(errno));
  }
}


SocketUdp::SocketUdp(SocketAddress const &bindAddress)
  : Socket(bindAddress.priv->Family(), SOCK_DGRAM, IPPROTO_UDP)
{
  auto const sockAddr = bindAddress.priv->SockAddrUdp();
  if(::bind(m_fd, sockAddr.addr, sockAddr.addrLen)) {
    throw std::runtime_error("failed to bind socket on address "
                             + std::to_string(bindAddress) + ": "
                             + std::to_string(errno));
  }
}

void SocketUdp::Transmit(char const *data, size_t size,
  SocketAddress const &dstAddress)
{
  auto const sockAddr = dstAddress.priv->SockAddrUdp();
  if(size != ::sendto(m_fd, data, size, 0, sockAddr.addr, sockAddr.addrLen)) {
    throw std::runtime_error("failed to transmit: "
                             + std::to_string(errno));
  }
}


SocketTcpClient::SocketTcpClient(SocketAddress const &connectAddress)
  : Socket(connectAddress.priv->Family(), SOCK_STREAM, IPPROTO_TCP)
{
  auto const sockAddr = connectAddress.priv->SockAddrTcp();
  if(::connect(m_fd, sockAddr.addr, sockAddr.addrLen)) {
    throw std::runtime_error("failed to connect: "
                             + std::to_string(errno));
  }
}

void SocketTcpClient::Transmit(const char *data, size_t size)
{
  if(size != ::send(m_fd, data, size, 0)) {
    throw std::runtime_error("failed to transmit: "
                             + std::to_string(errno));
  }
}

SocketTcpClient::SocketTcpClient(int fd)
  : Socket(fd)
{
}


SocketTcpServer::SocketTcpServer(const SocketAddress &bindAddress)
  : Socket(bindAddress.priv->Family(), SOCK_STREAM, IPPROTO_TCP)
{
  static int const opt = 1;
  if (::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    throw std::runtime_error("failed to set socket reuse :"
                             + std::to_string(errno));
  }

  auto const sockAddr = bindAddress.priv->SockAddrTcp();
  if(::bind(m_fd, sockAddr.addr, sockAddr.addrLen)) {
    throw std::runtime_error("failed to bind socket on address "
                             + std::to_string(bindAddress) + ": "
                             + std::to_string(errno));
  }
}

SocketTcpClient SocketTcpServer::Listen()
{
  if(::listen(m_fd, 1)) {
    throw std::runtime_error("failed to listen: "
                             + std::to_string(errno));
  }
  return SocketTcpClient(accept(m_fd, nullptr, nullptr));
}
