#include "socket.h"
#include "socket_address_priv.h" // for SocketAddress::SocketAddressPriv

#ifdef _WIN32
# include <winsock2.h> // for ::socket
# pragma comment(lib, "Ws2_32.lib")
#else
# include <arpa/inet.h> // for IPPROTO_UDP
# include <sys/select.h> // for fd_set
# include <sys/socket.h> // for ::socket
# include <unistd.h> // for ::close
#endif // _WIN32

#include <cstring> // for std::strerror
#include <stdexcept> // for std::runtime_error

namespace {
  fd_set ToFdSet(int fd)
  {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    return fds;
  }

  timeval ToTimeval(Socket::Time time)
  {
    using namespace std::chrono;

    auto const usec = duration_cast<microseconds>(time).count();

    return {
      static_cast<decltype(timeval::tv_sec)>(usec / 1000000U)
    , static_cast<decltype(timeval::tv_usec)>(usec % 1000000U)
    };
  }

  /// @return  0: timed out, <0: fd closed, >0: readable
  int SelectRead(int fd, Socket::Time timeout)
  {
    auto rfds = ToFdSet(fd);
    if(timeout > Socket::Time(0)) {
      timeval tv = ToTimeval(timeout);
      return ::select(fd + 1, &rfds, nullptr, nullptr, &tv);
    } else {
      return ::select(fd + 1, &rfds, nullptr, nullptr, nullptr);
    }
  }

  /// @return  0: timed out, <0: fd closed, >0: writable
  int SelectWrite(int fd, Socket::Time timeout)
  {
    auto wfds = ToFdSet(fd);
    if(timeout > Socket::Time(0)) {
      timeval tv = ToTimeval(timeout);
      return ::select(fd + 1, nullptr, &wfds, nullptr, &tv);
    } else {
      return ::select(fd + 1, nullptr, &wfds, nullptr, nullptr);
    }
  }
} // unnamed namespace

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

size_t Socket::Receive(char *data, size_t size, Time timeout)
{
  if(int const result = SelectRead(m_fd, timeout)) {
    if(result < 0) {
      throw std::runtime_error("failed to receive: "
                               + std::string(std::strerror(errno)));
    } else if(auto const received = ::recv(m_fd, data, size, 0)) {
      return received;
    } else {
      throw std::runtime_error("connection closed");
    }
  } else {
    // timeout exceeded
    return 0U;
  }
}

std::tuple<size_t, SocketAddress> Socket::ReceiveFrom(char *data, size_t size,
  Time timeout)
{
  if(int const result = SelectRead(m_fd, timeout)) {
    if(result < 0) {
      throw std::runtime_error("failed to receive from: "
                               + std::string(std::strerror(errno)));
    }
  } else {
    // timeout exceeded
    return {0U, SocketAddress()};
  }

  auto ss = std::make_shared<SocketAddressStorage>();

  auto const received = ::recvfrom(m_fd, data, size, 0, ss->Addr(), ss->AddrLen());

  return {received, SocketAddress(std::move(ss))};
}

Socket::Socket(int family, int type, int protocol)
  : m_socketGuard() // must be created before call to ::socket
  , m_fd(::socket(family, type, protocol))
{
  if(m_fd < 0) {
    throw std::runtime_error("failed to create socket: "
                             + std::string(std::strerror(errno)));
  }
}

Socket::Socket(int fd)
  : m_fd(fd)
{
  if(m_fd < 0) {
    throw std::runtime_error("failed to create socket: "
                             + std::string(std::strerror(errno)));
  }
}


SocketUdp::SocketUdp(SocketAddress const &bindAddress)
  : Socket(bindAddress.priv->Family(), SOCK_DGRAM, IPPROTO_UDP)
{
  auto const sockAddr = bindAddress.priv->SockAddrUdp();
  if(::bind(m_fd, sockAddr.addr, sockAddr.addrLen)) {
    throw std::runtime_error("failed to bind socket on address "
                             + std::to_string(bindAddress) + ": "
                             + std::strerror(errno));
  }
}

void SocketUdp::SendTo(char const *data, size_t size,
  SocketAddress const &dstAddress)
{
  auto const sockAddr = dstAddress.priv->SockAddrUdp();
  if(size != ::sendto(m_fd, data, size, 0, sockAddr.addr, sockAddr.addrLen)) {
    throw std::runtime_error("failed to send: "
                             + std::string(std::strerror(errno)));
  }
}


SocketTcpClient::SocketTcpClient(SocketAddress const &connectAddress)
  : Socket(connectAddress.priv->Family(), SOCK_STREAM, IPPROTO_TCP)
{
  auto const sockAddr = connectAddress.priv->SockAddrTcp();
  if(::connect(m_fd, sockAddr.addr, sockAddr.addrLen)) {
    throw std::runtime_error("failed to connect: "
                             + std::string(std::strerror(errno)));
  }
}

void SocketTcpClient::Send(const char *data, size_t size,
  Time timeout)
{
  auto error = []() -> std::runtime_error {
    return std::runtime_error("failed to send: "
                              + std::string(std::strerror(errno)));
  };

  if(int const result = SelectWrite(m_fd, timeout)) {
    if(result < 0) {
      throw error();
    } else if(size != ::send(m_fd, data, size, 0)) {
      throw error();
    }
  } else {
    throw std::runtime_error("send timed out");
  }
}

SocketTcpClient::SocketTcpClient(int fd)
  : Socket(fd)
{
}


SocketTcpServer::SocketTcpServer(const SocketAddress &bindAddress)
  : Socket(bindAddress.priv->Family(), SOCK_STREAM, IPPROTO_TCP)
{
  static char const opt = 1;
  if (::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    throw std::runtime_error("failed to set socket address reuse :"
                             + std::string(std::strerror(errno)));
  }

  auto const sockAddr = bindAddress.priv->SockAddrTcp();
  if(::bind(m_fd, sockAddr.addr, sockAddr.addrLen)) {
    throw std::runtime_error("failed to bind socket on address "
                             + std::to_string(bindAddress) + ": "
                             + std::strerror(errno));
  }
}

std::tuple<SocketTcpClient, SocketAddress> SocketTcpServer::Listen(Time timeout)
{
  auto error = []() -> std::runtime_error {
    return std::runtime_error("failed to listen: "
                              + std::string(std::strerror(errno)));
  };

  if(::listen(m_fd, 1)) {
    throw error();
  }

  if(int const result = SelectRead(m_fd, timeout)) {
    if(result < 0) {
      throw error();
    }
  } else {
    throw std::runtime_error("listen timed out");
  }

  auto ss = std::make_shared<SocketAddressStorage>();

  auto client = SocketTcpClient(::accept(m_fd, ss->Addr(), ss->AddrLen()));

  return {std::move(client), SocketAddress(std::move(ss))};
}
