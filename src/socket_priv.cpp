#include "socket_priv.h"
#include "error_code.h" // for SocketError

#ifndef _WIN32
# include <fcntl.h> // for fcntl
# include <poll.h> // for pollfd
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

static int const sendFlags =
#ifdef MSG_NOSIGNAL
    MSG_NOSIGNAL;
#else
    0;
#endif // MSG_NOSIGNAL

void CloseSocket(SOCKET fd)
{
#ifdef _WIN32
  (void)::closesocket(fd);
#else
  (void)::close(fd);
#endif // _WIN32
}

int SetNonBlocking(SOCKET fd)
{
#ifdef _WIN32
  unsigned long enable = 1U;
  return ::ioctlsocket(fd, static_cast<int>(FIONBIO), &enable);
#else
  int const flags = ::fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    return flags;
  }
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif // _WIN32
}

int DoPoll(Duration timeout, pollfd pfd)
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

bool Poll(Duration timeout, pollfd pfd, char const *errorMessage)
{
  if(auto const result = DoPoll(timeout, pfd)) {
    if(result < 0) {
      throw std::system_error(SocketError(), errorMessage);
    }
    return true; // read/write ready
  }
  return false; // timeout exceeded
}

struct Deadline
{
  Duration remaining;
  std::chrono::time_point<std::chrono::steady_clock> lastStart;

  Deadline(Duration timeout)
    : remaining(timeout)
  {
    if(remaining.count() >= 0) {
      lastStart = std::chrono::steady_clock::now();
    }
  }

  bool TimeLeft()
  {
    if(remaining.count() >= 0) {
      auto const now = std::chrono::steady_clock::now();

      remaining -= std::chrono::duration_cast<Duration>(now - lastStart);
      lastStart = now;

      return (remaining.count() > 0);
    }
    return true;
  }
};

} // unnamed namespace

SocketPriv::SocketPriv(int family, int type, int protocol)
  : guard() // must be created before call to ::socket
  , fd(::socket(family, type, protocol))
  , isBlocking(true)
{
  if(fd == fdInvalid) {
    throw std::system_error(SocketError(), "failed to create socket");
  }
}

SocketPriv::SocketPriv(SOCKET fd)
  : fd(fd)
  , isBlocking(true)
{
  if(fd == fdInvalid) {
    throw std::system_error(SocketError(), "failed to accept socket");
  }
}

SocketPriv::SocketPriv(SocketPriv &&other) noexcept
  : fd(other.fd)
  , isBlocking(other.isBlocking)
{
  other.fd = fdInvalid;
}

SocketPriv::~SocketPriv()
{
  if(fd != fdInvalid) {
    CloseSocket(fd);
  }
}

// used for both UDP and TCP
size_t SocketPriv::Receive(char *data, size_t size, Duration timeout)
{
  if(!PollReadable(timeout)) {
    return 0U; // timeout exceeded
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

// used for UDP only
std::pair<size_t, std::shared_ptr<SockAddrStorage>>
SocketPriv::ReceiveFrom(char *data, size_t size, Duration timeout)
{
  if(!PollReadable(timeout)) {
    return {0U, nullptr}; // timeout exceeded
  }

  static int const flags = 0;
  auto sas = std::make_shared<SockAddrStorage>();
  auto const received = ::recvfrom(fd,
                                   data, size,
                                   flags,
                                   sas->Addr(), sas->AddrLen());
  if(received < 0) {
    throw std::system_error(SocketError(), "failed to receive");
  }
  return {static_cast<size_t>(received), std::move(sas)};
}

// TCP send will block regularly, if:
//   the user enqueues faster than the NIC can send or the peer can process
//   network losses/delay causes retransmissions
// causing the OS send buffer to fill up
size_t SocketPriv::Send(char const *data, size_t size, Duration timeout)
{
  // utilize the user-provided timeout to send the max amount of data
  size_t sent = 0U;
  Deadline deadline(timeout);
  do {
    sent += SendSome(data + sent, size - sent, deadline.remaining);
  } while((sent < size) && deadline.TimeLeft());

  assert((sent == size) || (timeout.count() >= 0));
  return sent;
}

size_t SocketPriv::SendSome(char const *data, size_t size, Duration timeout)
{
  if(!PollWritable(timeout)) {
    return 0U; // timeout exceeded
  }

  auto const sent = ::send(fd,
                           data, size,
                           sendFlags);
  if(sent < 0) {
    throw std::system_error(SocketError(), "failed to send");
  } else if((sent == 0) && (size > 0U)) {
    throw std::logic_error("unexpected send result");
  }
  return static_cast<size_t>(sent);
}

// UDP send will block only rarely,
// if the user enqueues faster than the NIC can send
// causing the OS send buffer to fill up
size_t SocketPriv::SendTo(char const *data, size_t size,
    SockAddrView const &dstAddr, Duration timeout)
{
  if(!PollWritable(timeout)) {
    return 0U; // timeout exceeded
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
    throw std::logic_error("unexpected UDP send result");
  }
  return static_cast<size_t>(sent);
}

void SocketPriv::Connect(SockAddrView const &connectAddr)
{
  if(::connect(fd, connectAddr.addr, connectAddr.addrLen)) {
    auto const error = SocketError(); // cache before calling to_string
    throw std::system_error(error,
          "failed to connect to " + to_string(connectAddr));
  }
}

void SocketPriv::Bind(SockAddrView const &bindAddr)
{
  if(::bind(fd, bindAddr.addr, bindAddr.addrLen)) {
    auto const error = SocketError(); // cache before calling to_string
    throw std::system_error(error,
          "failed to bind socket to address " + to_string(bindAddr));
  }
}

void SocketPriv::Listen()
{
  static int const backlog = 128;
  if(::listen(fd, backlog)) {
    throw std::system_error(SocketError(), "failed to listen");
  }
}

std::pair<std::unique_ptr<SocketPriv>, std::shared_ptr<SockAddrStorage>>
SocketPriv::Accept(Duration timeout)
{
  if(!Poll(timeout,
           pollfd{fd, POLLIN, 0},
           "failed to wait for accept")) {
    throw std::runtime_error("accept timed out");
  }

  auto sas = std::make_shared<SockAddrStorage>();

  auto client = std::make_unique<SocketPriv>(
    ::accept(fd, sas->Addr(), sas->AddrLen()));

  return {std::move(client), std::move(sas)};
}

void SocketPriv::SetSockOptNonBlocking()
{
  if(SetNonBlocking(fd)) {
    throw std::system_error(SocketError(),
        "failed to set socket option non-blocking");
  }
}

void SocketPriv::SetSockOptReuseAddr()
{
  SetSockOpt(SO_REUSEADDR, 1, "failed to set socket option address reuse");
}

void SocketPriv::SetSockOptBroadcast()
{
  SetSockOpt(SO_BROADCAST, 1, "failed to set socket option broadcast");
}

void SocketPriv::SetSockOptNoSigPipe()
{
#ifdef SO_NOSIGPIPE
  SetSockOpt(SO_NOSIGPIPE, 1, "failed to set socket option non-SIGPIPE");
#endif // SO_NOSIGPIPE
}

void SocketPriv::SetSockOpt(int id, int value, char const *errorMessage)
{
  if(::setsockopt(fd, SOL_SOCKET, id,
                  reinterpret_cast<char const *>(&value), sizeof(value))) {
    throw std::system_error(SocketError(), errorMessage);
  }
}

size_t SocketPriv::GetSockOptRcvBuf() const
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

std::shared_ptr<SockAddrStorage> SocketPriv::GetSockName() const
{
  auto sas = std::make_shared<SockAddrStorage>();

  if(::getsockname(fd, sas->Addr(), sas->AddrLen())) {
    throw std::system_error(SocketError(), "failed to get socket address");
  }

  return sas;
}

std::shared_ptr<SockAddrStorage> SocketPriv::GetPeerName() const
{
  auto sas = std::make_shared<SockAddrStorage>();

  if(::getpeername(fd, sas->Addr(), sas->AddrLen())) {
    throw std::system_error(SocketError(), "failed to get peer address");
  }

  return sas;
}

bool SocketPriv::PollReadable(Duration timeout)
{
  if(NeedsPoll(timeout)) {
    return Poll(timeout,
                pollfd{fd, POLLIN, 0},
                "failed to wait for socket input");
  }
  return true;
}

bool SocketPriv::PollWritable(Duration timeout)
{
  if(NeedsPoll(timeout)) {
    return Poll(timeout,
                pollfd{fd, POLLOUT, 0},
                "failed to wait for socket writable");
  }
  return true;
}

bool SocketPriv::NeedsPoll(Duration timeout)
{
  if(isBlocking && (timeout.count() >= 0)) {
    SetSockOptNonBlocking();
    isBlocking = false;
  }
  return !isBlocking;
}

} // namespace sockpuppet
