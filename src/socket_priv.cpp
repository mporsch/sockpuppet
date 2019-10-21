#include "socket_priv.h"
#include "error_code.h" // for SocketError

#ifndef _WIN32
# include <sys/socket.h> // for ::socket
# include <unistd.h> // for ::close
#endif // _WIN32

#include <cassert> // for assert

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

  struct Deadline
  {
    Socket::Duration remaining;
    std::chrono::time_point<std::chrono::steady_clock> lastStart;

    Deadline(Socket::Duration timeout)
      :remaining(timeout)
    {
      if(remaining.count() >= 0) {
        lastStart = std::chrono::steady_clock::now();
      }
    }

    bool TimeLeft()
    {
      if(remaining.count() >= 0) {
        auto const now = std::chrono::steady_clock::now();

        remaining -= std::chrono::duration_cast<Socket::Duration>(now - lastStart);
        lastStart = now;

        return (remaining.count() > 0);
      }
      return true;
    }
  };

} // unnamed namespace

Socket::SocketPriv::SocketPriv(int family, int type, int protocol)
  : guard() // must be created before call to ::socket
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
  if(auto const result = PollRead(timeout)) {
    if(result < 0) {
      throw std::system_error(SocketError(), "failed to receive");
    }
  } else {
    // timeout exceeded
    return 0U;
  }

  static int const flags = 0;
  auto const received = ::recv(fd,
                               data, size,
                               flags);
  if(received < 0) {
    throw std::system_error(SocketError(), "failed to receive");
  } else if(received == 0) {
    throw std::runtime_error("connection closed");
  }
  return static_cast<size_t>(received);
}

std::tuple<size_t, std::shared_ptr<SockAddrStorage>>
Socket::SocketPriv::ReceiveFrom(char *data, size_t size, Duration timeout)
{
  if(auto const result = PollRead(timeout)) {
    if(result < 0) {
      throw std::system_error(SocketError(), "failed to receive");
    }
  } else {
    // timeout exceeded
    return std::tuple<size_t, std::shared_ptr<SockAddrStorage>>{
      0U
    , nullptr
    };
  }

  static int const flags = 0;
  auto sas = std::make_shared<SockAddrStorage>();
  auto const received = ::recvfrom(fd,
                                   data, size,
                                   flags,
                                   sas->Addr(), sas->AddrLen());
  if(received < 0) {
    throw std::system_error(SocketError(), "failed to receive");
  } else if(received == 0) {
    throw std::logic_error("unexpected UDP receive result");
  }
  return std::tuple<size_t, std::shared_ptr<SockAddrStorage>>{
    static_cast<size_t>(received)
  , std::move(sas)
  };
}

size_t Socket::SocketPriv::Send(char const *data, size_t size, Duration timeout)
{
  size_t sent = 0U;
  Deadline deadline(timeout);
  do
  {
    sent += SendIteration(data + sent, size - sent, deadline.remaining);
  } while((sent < size) && deadline.TimeLeft());

  assert((sent == size) || (timeout.count() >= 0));
  return sent;
}

size_t Socket::SocketPriv::SendIteration(char const *data, size_t size, Duration timeout)
{
  // TCP sockets will block regularly, if:
  //   the user enqueues faster than the NIC can send or the peer can process
  //   network losses/delay causes retransmissions
  // causing the OS send buffer to fill up

  if(auto const result = PollWrite(timeout)) {
    if(result < 0) {
      throw std::system_error(SocketError(), "failed to send");
    }
  } else {
    // timeout exceeded
    return 0U;
  }

  static int const flags = 0;
  auto const sent = ::send(fd,
                           data, size,
                           flags);
  if(sent < 0) {
    throw std::system_error(SocketError(), "failed to send");
  } else if((sent == 0) && (size > 0U)) {
    throw std::logic_error("unexpected send result");
  }
  return static_cast<size_t>(sent);
}

size_t Socket::SocketPriv::SendTo(char const *data, size_t size,
    SockAddrView const &dstAddr, Duration timeout)
{
  // UDP sockets will block only rarely,
  // if the user enqueues faster than the NIC can send
  // causing the OS send buffer to fill up

  if(auto const result = PollWrite(timeout)) {
    if(result < 0) {
      throw std::system_error(SocketError(), "failed to send");
    }
  } else {
    // timeout exceeded
    return 0U;
  }

  static int const flags = 0;
  auto const sent = ::sendto(fd,
                             data, size,
                             flags,
                             dstAddr.addr, dstAddr.addrLen);
  if(sent < 0) {
    auto const error = SocketError(); // cache before calling to_string
    throw std::system_error(error,
          "failed to send to " + to_string(dstAddr));
  } else if(static_cast<size_t>(sent) != size) {
    throw std::logic_error("unexpected send result");
  }
  return static_cast<size_t>(sent);
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
           std::shared_ptr<SockAddrStorage>>
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
                    std::shared_ptr<SockAddrStorage>>{
    std::move(client)
  , std::move(sas)
  };
}

void Socket::SocketPriv::SetSockOptNonBlocking()
{
  unsigned long enable = 1U;
  if(::ioctlsocket(fd, static_cast<int>(FIONBIO), &enable)) {
    throw std::system_error(SocketError(),
        "failed to set socket option non-blocking");
  }
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
  if(::setsockopt(fd, SOL_SOCKET, id,
                  reinterpret_cast<char const *>(&value), sizeof(value))) {
    throw std::system_error(SocketError(), errorMessage);
  }
}

size_t Socket::SocketPriv::GetSockOptRcvBuf() const
{
  int value;
  {
    auto size = static_cast<socklen_t>(sizeof(value));
    if(::getsockopt(fd, SOL_SOCKET, SO_RCVBUF,
                    reinterpret_cast<char *>(&value), &size) ||
        size != sizeof(value)) {
      throw std::system_error(SocketError(), "failed to get socket receive buffer size");
    }
  }

  if(value < 0) {
    throw std::logic_error("unexpected receive buffer size");
  }
  return static_cast<size_t>(value);
}

std::shared_ptr<SockAddrStorage> Socket::SocketPriv::GetSockName() const
{
  auto sas = std::make_shared<SockAddrStorage>();

  if(::getsockname(fd, sas->Addr(), sas->AddrLen())) {
    throw std::system_error(SocketError(), "failed to get socket address");
  }

  return sas;
}

std::shared_ptr<SockAddrStorage> Socket::SocketPriv::GetPeerName() const
{
  auto sas = std::make_shared<SockAddrStorage>();

  if(::getpeername(fd, sas->Addr(), sas->AddrLen())) {
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
