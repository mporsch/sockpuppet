#include "socket_priv.h"

#ifdef _WIN32
# include <winsock2.h> // for ::socket
# pragma comment(lib, "Ws2_32.lib")
#else
# include <sys/select.h> // for fd_set
# include <sys/socket.h> // for ::socket
# include <unistd.h> // for ::close
#endif // _WIN32

#include <cstring> // for std::strerror
#include <stdexcept> // for std::runtime_error

namespace {
  fd_set ToFdSet(SOCKET fd)
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
} // unnamed namespace

Socket::SocketPriv::SocketPriv(int family, int type, int protocol)
  : socketGuard() // must be created before call to ::socket
  , fd(::socket(family, type, protocol))
{
  if(fd < 0) {
    throw std::runtime_error("failed to create socket: "
                             + std::string(std::strerror(errno)));
  }
}

Socket::SocketPriv::SocketPriv(SOCKET fd)
  : fd(fd)
{
  if(fd < 0) {
    throw std::runtime_error("failed to create socket: "
                             + std::string(std::strerror(errno)));
  }
}

Socket::SocketPriv::~SocketPriv()
{
  if(fd >= 0) {
#ifdef _WIN32
    (void)closesocket(fd);
#else
    (void)::close(fd);
#endif // _WIN32
  }
}

size_t Socket::SocketPriv::Receive(char *data, size_t size,
  Socket::Time timeout)
{
  if(auto const result = SelectRead(timeout)) {
    if(result < 0) {
      throw std::runtime_error("failed to receive: "
                               + std::string(std::strerror(errno)));
    } else if(auto const received = ::recv(fd, data, size, 0)) {
      return received;
    } else {
      throw std::runtime_error("connection closed");
    }
  } else {
    // timeout exceeded
    return 0U;
  }
}

std::tuple<size_t, std::shared_ptr<SocketAddressStorage>>
Socket::SocketPriv::ReceiveFrom(char *data, size_t size,
  Time timeout)
{
  if(auto const result = SelectRead(timeout)) {
    if(result < 0) {
      throw std::runtime_error("failed to receive from: "
                               + std::string(std::strerror(errno)));
    }
  } else {
    // timeout exceeded
    return {0U, nullptr};
  }

  auto ss = std::make_shared<SocketAddressStorage>();
  auto const received = ::recvfrom(fd, data, size, 0,
                                   ss->Addr(), ss->AddrLen());
  return {received, std::move(ss)};
}

void Socket::SocketPriv::Send(char const *data, size_t size,
  Socket::Time timeout)
{
  auto error = []() -> std::runtime_error {
    return std::runtime_error("failed to send: "
                              + std::string(std::strerror(errno)));
  };

  if(auto const result = SelectWrite(timeout)) {
    if(result < 0) {
      throw error();
    } else if(size != ::send(fd, data, size, 0)) {
      throw error();
    }
  } else {
    throw std::runtime_error("send timed out");
  }
}

void Socket::SocketPriv::SendTo(char const *data, size_t size,
  SockAddr const &dstAddr)
{
  if(size != ::sendto(fd, data, size, 0,
                      dstAddr.addr, dstAddr.addrLen)) {
    throw std::runtime_error("failed to send to "
                             + std::to_string(dstAddr)
                             + ": "
                             + std::string(std::strerror(errno)));
  }
}

void Socket::SocketPriv::Connect(SockAddr const &connectAddr)
{
  if(::connect(fd, connectAddr.addr, connectAddr.addrLen)) {
    throw std::runtime_error("failed to connect to "
                             + std::to_string(connectAddr)
                             + ": "
                             + std::string(std::strerror(errno)));
  }
}

void Socket::SocketPriv::Bind(SockAddr const &sockAddr)
{
  if(::bind(fd, sockAddr.addr, sockAddr.addrLen)) {
    throw std::runtime_error("failed to bind socket on address "
                             + std::to_string(sockAddr) + ": "
                             + std::strerror(errno));
  }
}

std::tuple<std::unique_ptr<Socket::SocketPriv>,
           std::shared_ptr<SocketAddressStorage>>
Socket::SocketPriv::Listen(Time timeout)
{
  auto error = []() -> std::runtime_error {
    return std::runtime_error("failed to listen: "
                              + std::string(std::strerror(errno)));
  };

  if(::listen(fd, 1)) {
    throw error();
  }

  if(auto const result = SelectRead(timeout)) {
    if(result < 0) {
      throw error();
    }
  } else {
    throw std::runtime_error("listen timed out");
  }

  auto ss = std::make_shared<SocketAddressStorage>();

  auto client = std::make_unique<SocketPriv>(
    ::accept(fd, ss->Addr(), ss->AddrLen()));

  return {std::move(client), std::move(ss)};
}

void Socket::SocketPriv::SetSockOptReuseAddr()
{
  static int const opt = 1;
  if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<char const *>(&opt), sizeof(opt))) {
    throw std::runtime_error("failed to set socket address reuse: "
                             + std::string(std::strerror(errno)));
  }
}

/// @return  0: timed out, <0: fd closed, >0: fd readable
int Socket::SocketPriv::SelectRead(Socket::Time timeout)
{
  // unix expects the first ::select parameter to be the
  // highest-numbered file descriptor in any of the three sets, plus 1
  // windows ignores the parameter

  auto rfds = ToFdSet(fd);
  if(timeout > Socket::Time(0U)) {
    timeval tv = ToTimeval(timeout);
    return ::select(static_cast<int>(fd + 1), &rfds, nullptr, nullptr, &tv);
  } else {
    return ::select(static_cast<int>(fd + 1), &rfds, nullptr, nullptr, nullptr);
  }
}

/// @return  0: timed out, <0: fd closed, >0: fd writable
int Socket::SocketPriv::SelectWrite(Socket::Time timeout)
{
  auto wfds = ToFdSet(fd);
  if(timeout > Socket::Time(0U)) {
    timeval tv = ToTimeval(timeout);
    return ::select(static_cast<int>(fd + 1), nullptr, &wfds, nullptr, &tv);
  } else {
    return ::select(static_cast<int>(fd + 1), nullptr, &wfds, nullptr, nullptr);
  }
}
