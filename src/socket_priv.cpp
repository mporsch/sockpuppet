#include "socket_priv.h"

#ifndef _WIN32
# include <sys/socket.h> // for ::socket
# include <unistd.h> // for ::close
#endif // _WIN32

#include <cstring> // for std::strerror
#include <stdexcept> // for std::runtime_error
#include <string> // for std::string

namespace sockpuppet {

namespace {
  fd_set ToFdSet(SOCKET fd)
  {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    return fds;
  }

  void SetInvalid(SOCKET &fd)
  {
    fd = static_cast<SOCKET>(-1);
  }

  bool IsValid(SOCKET const &fd)
  {
    return (fd >= 0);
  }
} // unnamed namespace

Socket::SocketPriv::SocketPriv(int family, int type, int protocol)
  : socketGuard() // must be created before call to ::socket
  , fd(::socket(family, type, protocol))
{
  if(!IsValid(fd)) {
    throw std::runtime_error("failed to create socket: "
                             + std::string(std::strerror(errno)));
  }
}

Socket::SocketPriv::SocketPriv(SOCKET fd)
  : fd(fd)
{
  if(!IsValid(fd)) {
    throw std::runtime_error("failed to create socket: "
                             + std::string(std::strerror(errno)));
  }
}

Socket::SocketPriv::SocketPriv(SocketPriv &&other) noexcept
  : fd(other.fd)
{
  SetInvalid(other.fd);
}

Socket::SocketPriv::~SocketPriv()
{
  if(IsValid(fd)) {
#ifdef _WIN32
    (void)closesocket(fd);
#else
    (void)::close(fd);
#endif // _WIN32
  }
}

size_t Socket::SocketPriv::Receive(char *data, size_t size, Time timeout)
{
  if(timeout.count() > 0U) {
    if(auto const result = SelectRead(timeout)) {
      if(result < 0) {
        throw std::runtime_error("failed to receive: "
                                 + std::string(std::strerror(errno)));
      }
    } else {
      // timeout exceeded
      return 0U;
    }
  }

  auto const received = ::recv(fd, data, size, 0);
  if(received > 0) {
    return static_cast<size_t>(received);
  } else {
    throw std::runtime_error("connection closed");
  }
}

std::tuple<size_t, std::shared_ptr<SocketAddress::SocketAddressPriv>>
Socket::SocketPriv::ReceiveFrom(char *data, size_t size, Time timeout)
{
  if(timeout.count() > 0U) {
    if(auto const result = SelectRead(timeout)) {
      if(result < 0) {
        throw std::runtime_error("failed to receive from: "
                                 + std::string(std::strerror(errno)));
      }
    } else {
      // timeout exceeded
      return std::tuple<size_t, std::shared_ptr<SocketAddress::SocketAddressPriv>>{
        0U
      , nullptr
      };
    }
  }

  auto ss = std::make_shared<SocketAddressStorage>();
  auto const received = ::recvfrom(fd, data, size, 0,
                                   ss->Addr(), ss->AddrLen());
  return std::tuple<size_t, std::shared_ptr<SocketAddress::SocketAddressPriv>>{
    received
  , std::move(ss)
  };
}

void Socket::SocketPriv::Send(char const *data, size_t size, Time timeout)
{
  auto error = []() -> std::runtime_error {
    return std::runtime_error("failed to send: "
                              + std::string(std::strerror(errno)));
  };

  if(timeout.count() > 0U) {
    if(auto const result = SelectWrite(timeout)) {
      if(result < 0) {
        throw error();
      }
    } else {
      throw std::runtime_error("send timed out");
    }
  }

  if(size != ::send(fd, data, size, 0)) {
    throw error();
  }
}

void Socket::SocketPriv::SendTo(char const *data, size_t size,
    SockAddr const &dstAddr)
{
  if(size != ::sendto(fd, data, size, 0,
                      dstAddr.addr, dstAddr.addrLen)) {
    throw std::runtime_error("failed to send to "
                             + to_string(dstAddr)
                             + ": "
                             + std::string(std::strerror(errno)));
  }
}

void Socket::SocketPriv::Connect(SockAddr const &connectAddr)
{
  if(::connect(fd, connectAddr.addr, connectAddr.addrLen)) {
    throw std::runtime_error("failed to connect to "
                             + to_string(connectAddr)
                             + ": "
                             + std::string(std::strerror(errno)));
  }
}

void Socket::SocketPriv::Bind(SockAddr const &sockAddr)
{
  if(::bind(fd, sockAddr.addr, sockAddr.addrLen)) {
    throw std::runtime_error("failed to bind socket on address "
                             + to_string(sockAddr)
                             + ": "
                             + std::strerror(errno));
  }
}

void Socket::SocketPriv::Listen()
{
  if(::listen(fd, 1)) {
    throw std::runtime_error("failed to listen: "
                             + std::string(std::strerror(errno)));
  }
}

std::tuple<std::unique_ptr<Socket::SocketPriv>,
           std::shared_ptr<SocketAddress::SocketAddressPriv>>
Socket::SocketPriv::Accept(Time timeout)
{
  if(timeout.count() > 0U) {
    if(auto const result = SelectRead(timeout)) {
      if(result < 0) {
        throw std::runtime_error("failed to accept: "
                                 + std::string(std::strerror(errno)));
      }
    } else {
      throw std::runtime_error("accept timed out");
    }
  }

  auto ss = std::make_shared<SocketAddressStorage>();

  auto client = std::make_unique<SocketPriv>(
    ::accept(fd, ss->Addr(), ss->AddrLen()));

  return std::tuple<std::unique_ptr<Socket::SocketPriv>,
                    std::shared_ptr<SocketAddress::SocketAddressPriv>>{
    std::move(client)
  , std::move(ss)
  };
}

void Socket::SocketPriv::SetSockOptReuseAddr()
{
  SetSockOpt(SO_REUSEADDR, 1, "address reuse");
}

void Socket::SocketPriv::SetSockOptBroadcast()
{
  SetSockOpt(SO_BROADCAST, 1, "broadcast");
}

void Socket::SocketPriv::SetSockOpt(int id, int value,
    char const *name)
{
  if (::setsockopt(fd, SOL_SOCKET, id,
                   reinterpret_cast<char const *>(&value), sizeof(value))) {
    throw std::runtime_error("failed to set socket option "
                             + std::string(name)
                             + ": "
                             + std::strerror(errno));
  }
}

int Socket::SocketPriv::GetSockOptRcvBuf()
{
  int ret;
  auto size = static_cast<socklen_t>(sizeof(ret));
  if (::getsockopt(fd, SOL_SOCKET, SO_RCVBUF,
                   reinterpret_cast<char *>(&ret), &size) ||
      size != sizeof(ret)) {
    throw std::runtime_error("failed to get socket receive buffer size: "
                             + std::string(std::strerror(errno)));
  }
  return ret;
}

std::shared_ptr<SocketAddressStorage> Socket::SocketPriv::GetSockName()
{
  auto ss = std::make_shared<SocketAddressStorage>();

  if (::getsockname(fd, ss->Addr(), ss->AddrLen())) {
    throw std::runtime_error("failed to get socket address: "
                             + std::string(std::strerror(errno)));
  }

  return ss;
}

std::shared_ptr<SocketAddressStorage> Socket::SocketPriv::GetPeerName()
{
  auto ss = std::make_shared<SocketAddressStorage>();

  if (::getpeername(fd, ss->Addr(), ss->AddrLen())) {
    throw std::runtime_error("failed to get peer address: "
                             + std::string(std::strerror(errno)));
  }

  return ss;
}

int Socket::SocketPriv::SelectRead(Time timeout)
{
  auto rfds = ToFdSet(fd);
  return Select(&rfds, nullptr, timeout);
}

int Socket::SocketPriv::SelectWrite(Time timeout)
{
  auto wfds = ToFdSet(fd);
  return Select(nullptr, &wfds, timeout);
}

int Socket::SocketPriv::Select(fd_set *rfds, fd_set *wfds, Time timeout)
{
  // unix expects the first ::select parameter to be the
  // highest-numbered file descriptor in any of the three sets, plus 1
  // windows ignores the parameter

  auto tv = ToTimeval(timeout);
  return ::select(static_cast<int>(fd + 1), rfds, wfds, nullptr, &tv);
}

timeval Socket::SocketPriv::ToTimeval(Time time)
{
  using namespace std::chrono;

  auto const usec = duration_cast<microseconds>(time).count();

  return {
    static_cast<decltype(timeval::tv_sec)>(usec / 1000000U)
  , static_cast<decltype(timeval::tv_usec)>(usec % 1000000U)
  };
}

} // namespace sockpuppet
