#include "socket_priv.h"
#include "util.h" // for SocketError

#ifndef _WIN32
# include <sys/socket.h> // for ::socket
# include <unistd.h> // for ::close
#endif // _WIN32

#include <system_error> // for std::system_error

namespace sockpuppet {

namespace {
  static auto const fdInvalid =
    #ifdef _WIN32
      INVALID_SOCKET;
    #else
      SOCKET(-1);
    #endif // _WIN32

  void CloseSocket(SOCKET fd)
  {
#ifdef _WIN32
    (void)::closesocket(fd);
#else
    (void)::close(fd);
#endif // _WIN32
  }

  int Poll(pollfd pfd, Socket::Duration timeout)
  {
    using namespace std::chrono;

    auto const msec = static_cast<int>(
          duration_cast<milliseconds>(timeout).count());

#ifdef _WIN32
    return ::WSAPoll(&pfd, 1U, msec);
#else
    return ::poll(&pfd, 1U, msec);
#endif // _WIN32
  }
} // unnamed namespace

Socket::SocketPriv::SocketPriv(int family, int type, int protocol)
  : socketGuard() // must be created before call to ::socket
  , fd(::socket(family, type, protocol))
{
  if(fd == fdInvalid) {
    throw std::system_error(SocketError(), "failed to create socket");
  }
}

Socket::SocketPriv::SocketPriv(SOCKET fd)
  : fd(fd)
{
  if(fd == fdInvalid) {
    throw std::system_error(SocketError(), "failed to create socket");
  }
}

Socket::SocketPriv::SocketPriv(SocketPriv &&other) noexcept
  : fd(other.fd)
{
  other.fd = fdInvalid;
}

Socket::SocketPriv::~SocketPriv()
{
  if(fd != fdInvalid) {
    CloseSocket(fd);
  }
}

size_t Socket::SocketPriv::Receive(char *data, size_t size, Duration timeout)
{
  if(timeout.count() >= 0) {
    if(auto const result = PollRead(timeout)) {
      if(result < 0) {
        throw std::system_error(SocketError(), "failed to receive");
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
Socket::SocketPriv::ReceiveFrom(char *data, size_t size, Duration timeout)
{
  if(timeout.count() >= 0) {
    if(auto const result = PollRead(timeout)) {
      if(result < 0) {
        throw std::system_error(SocketError(), "failed to receive");
      }
    } else {
      // timeout exceeded
      return std::tuple<size_t, std::shared_ptr<SocketAddress::SocketAddressPriv>>{
        0U
      , nullptr
      };
    }
  }

  auto sas = std::make_shared<SockAddrStorage>();
  auto const received = ::recvfrom(fd, data, size, 0,
                                   sas->Addr(), sas->AddrLen());
  return std::tuple<size_t, std::shared_ptr<SocketAddress::SocketAddressPriv>>{
    received
  , std::move(sas)
  };
}

void Socket::SocketPriv::Send(char const *data, size_t size, Duration timeout)
{
  if(timeout.count() >= 0) {
    if(auto const result = PollWrite(timeout)) {
      if(result < 0) {
        std::system_error(SocketError(), "failed to send");
      }
    } else {
      throw std::runtime_error("send timed out");
    }
  }

  if(size != ::send(fd, data, size, 0)) {
    std::system_error(SocketError(), "failed to send");
  }
}

void Socket::SocketPriv::SendTo(char const *data, size_t size,
    SockAddrView const &dstAddr)
{
  if(size != ::sendto(fd, data, size, 0,
                      dstAddr.addr, dstAddr.addrLen)) {
    auto const error = SocketError(); // cache before calling to_string
    throw std::system_error(error,
          "failed to send to " + to_string(dstAddr));
  }
}

void Socket::SocketPriv::Connect(SockAddrView const &connectAddr)
{
  if(::connect(fd, connectAddr.addr, connectAddr.addrLen)) {
    auto const error = SocketError(); // cache before calling to_string
    throw std::system_error(error,
          "failed to connect to " + to_string(connectAddr));
  }
}

void Socket::SocketPriv::Bind(SockAddrView const &sockAddr)
{
  if(::bind(fd, sockAddr.addr, sockAddr.addrLen)) {
    auto const error = SocketError(); // cache before calling to_string
    throw std::system_error(error,
          "failed to bind socket to address " + to_string(sockAddr));
  }
}

void Socket::SocketPriv::Listen()
{
  if(::listen(fd, 1)) {
    throw std::system_error(SocketError(), "failed to listen");
  }
}

std::tuple<std::unique_ptr<Socket::SocketPriv>,
           std::shared_ptr<SocketAddress::SocketAddressPriv>>
Socket::SocketPriv::Accept(Duration timeout)
{
  if(timeout.count() >= 0) {
    if(auto const result = PollRead(timeout)) {
      if(result < 0) {
        throw std::system_error(SocketError(), "failed to accept");
      }
    } else {
      throw std::runtime_error("accept timed out");
    }
  }

  auto sas = std::make_shared<SockAddrStorage>();

  auto client = std::make_unique<SocketPriv>(
    ::accept(fd, sas->Addr(), sas->AddrLen()));

  return std::tuple<std::unique_ptr<Socket::SocketPriv>,
                    std::shared_ptr<SocketAddress::SocketAddressPriv>>{
    std::move(client)
  , std::move(sas)
  };
}

void Socket::SocketPriv::SetSockOptReuseAddr()
{
  SetSockOpt(SO_REUSEADDR, 1, "failed to set socket option address reuse");
}

void Socket::SocketPriv::SetSockOptBroadcast()
{
  SetSockOpt(SO_BROADCAST, 1, "failed to set socket option broadcast");
}

void Socket::SocketPriv::SetSockOpt(int id, int value, char const *errorMessage)
{
  if (::setsockopt(fd, SOL_SOCKET, id,
                   reinterpret_cast<char const *>(&value), sizeof(value))) {
    throw std::system_error(SocketError(), errorMessage);
  }
}

size_t Socket::SocketPriv::GetSockOptRcvBuf() const
{
  int value;
  {
    auto size = static_cast<socklen_t>(sizeof(value));
    if (::getsockopt(fd, SOL_SOCKET, SO_RCVBUF,
                     reinterpret_cast<char *>(&value), &size) ||
        size != sizeof(value)) {
      throw std::system_error(SocketError(), "failed to get socket receive buffer size");
    }
  }

  if(value < 0) {
    throw std::logic_error("invalid receive buffer size");
  }
  return static_cast<size_t>(value);
}

std::shared_ptr<SockAddrStorage> Socket::SocketPriv::GetSockName() const
{
  auto sas = std::make_shared<SockAddrStorage>();

  if (::getsockname(fd, sas->Addr(), sas->AddrLen())) {
    throw std::system_error(SocketError(), "failed to get socket address");
  }

  return sas;
}

std::shared_ptr<SockAddrStorage> Socket::SocketPriv::GetPeerName() const
{
  auto sas = std::make_shared<SockAddrStorage>();

  if (::getpeername(fd, sas->Addr(), sas->AddrLen())) {
    throw std::system_error(SocketError(), "failed to get peer address");
  }

  return sas;
}

int Socket::SocketPriv::PollRead(Duration timeout) const
{
  return Poll(pollfd{fd, POLLIN, 0}, timeout);
}

int Socket::SocketPriv::PollWrite(Duration timeout) const
{
  return Poll(pollfd{fd, POLLOUT, 0}, timeout);
}

} // namespace sockpuppet
