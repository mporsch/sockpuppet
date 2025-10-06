#include "socket_impl.h"
#include "error_code.h" // for SocketError

#ifndef _WIN32
# include <fcntl.h> // for ::fcntl
# include <sys/socket.h> // for ::socket
# include <unistd.h> // for ::close
#endif // _WIN32

#include <cassert> // for assert
#include <string_view> // for std::string_view

namespace sockpuppet {

namespace {

constexpr auto fdInvalid =
#ifdef _WIN32
    INVALID_SOCKET;
#else
    SOCKET(-1);
#endif // _WIN32

constexpr int sendFlags =
#ifdef MSG_NOSIGNAL
    MSG_NOSIGNAL | // avoid SIGPIPE on connection closed (in Linux)
#endif // MSG_NOSIGNAL
    0;

void CloseSocket(SOCKET fd)
{
#ifdef _WIN32
  (void)::closesocket(fd);
#else
  (void)::close(fd);
#endif // _WIN32
}

int DoSetBlocking(SOCKET fd, bool blocking)
{
#ifdef _WIN32
  unsigned long enable = (blocking ? 0U : 1U);
  return ::ioctlsocket(fd, static_cast<int>(FIONBIO), &enable);
#else
  int flags = ::fcntl(fd, F_GETFL, 0);
  if(flags == -1) {
    return flags;
  }
  flags = (blocking ? flags & ~O_NONBLOCK : flags | O_NONBLOCK);
  return ::fcntl(fd, F_SETFL, flags);
#endif // _WIN32
}

void SetBlocking(SOCKET fd, bool blocking, char const *errorMessage)
{
  if(DoSetBlocking(fd, blocking)) {
    throw std::system_error(SocketError(), errorMessage);
  }
}

void DoSetSockOpt(
    SOCKET fd, int level, int id,
    char const *opt, socklen_t optLen,
    char const *errorMessage)
{
  if(::setsockopt(fd, level, id, opt, optLen)) {
    throw std::system_error(SocketError(), errorMessage);
  }
}

void DoSetSockOpt(SOCKET fd, int id, int opt, char const *errorMessage)
{
    DoSetSockOpt(fd, SOL_SOCKET, id,
       reinterpret_cast<char const *>(&opt), sizeof(opt),
       errorMessage);
}

template<typename T>
T GetSockOpt(SOCKET fd, int id, char const *errorMessage)
{
  T value;
  socklen_t size = sizeof(value);
  if(::getsockopt(fd, SOL_SOCKET, id,
       reinterpret_cast<char *>(&value), &size) ||
     size != sizeof(value)) {
    throw std::system_error(SocketError(), errorMessage);
  }
  return value;
}

} // unnamed namespace

SocketImpl::SocketImpl(int family, int type, int protocol)
  : guard() // must be created before call to ::socket
  , fd(::socket(family, type, protocol))
{
  if(fd == fdInvalid) {
    throw std::system_error(SocketError(), "failed to create socket");
  }
}

SocketImpl::SocketImpl(SOCKET fd)
  : fd(fd)
{
  if(fd == fdInvalid) {
    throw std::system_error(SocketError(), "failed to accept socket");
  }
}

SocketImpl::SocketImpl(SocketImpl &&other) noexcept
  : fd(other.fd)
{
  other.fd = fdInvalid;
}

SocketImpl::~SocketImpl()
{
  if(fd != fdInvalid) {
    CloseSocket(fd);
  }
}

// used for TCP only
std::optional<size_t> SocketImpl::Receive(char *data, size_t size, Duration timeout)
{
  return sockpuppet::Receive(fd, data, size, timeout);
}

size_t SocketImpl::Receive(char *data, size_t size)
{
  return ReceiveNow(fd, data, size);
}

// used for UDP only
std::optional<std::pair<size_t, Address>>
SocketImpl::ReceiveFrom(char *data, size_t size, Duration timeout)
{
  if(!WaitReadable(fd, timeout)) {
    return {std::nullopt}; // timeout exceeded
  }
  return {ReceiveFrom(data, size)};
}

std::pair<size_t, Address>
SocketImpl::ReceiveFrom(char *data, size_t size)
{
  constexpr int flags = 0;
  auto sas = std::make_shared<SockAddrStorage>();
  auto received = ::recvfrom(fd,
                             data, size,
                             flags,
                             sas->Addr(), sas->AddrLen());
  if(received < 0) {
    throw std::system_error(SocketError(), "failed to receive");
  }
  return {
    static_cast<size_t>(received),
    Address(std::move(sas))
  };
}

// TCP send will block regularly, if:
//   the user enqueues faster than the NIC can send or the peer can process
//   network losses/delay causes retransmissions
// causing the OS send buffer to fill up
size_t SocketImpl::Send(char const *data, size_t size, Duration timeout)
{
  if(timeout.count() < 0) {
    return SendAll(fd, data, size);
  }
  if(timeout.count() == 0) {
    return SendTry(fd, data, size);
  }
  DeadlineLimited deadline(timeout);
  return sockpuppet::SendSome(fd, data, size, deadline);
}

size_t SocketImpl::SendSome(char const *data, size_t size)
{
  return SendNow(fd, data, size);
}

// UDP send will block only rarely,
// if the user enqueues faster than the NIC can send
// causing the OS send buffer to fill up
size_t SocketImpl::SendTo(char const *data, size_t size,
    SockAddrView const &dstAddr, Duration timeout)
{
  if(!WaitWritable(fd, timeout)) {
    return 0U; // timeout exceeded
  }
  return SendTo(data, size, dstAddr);
}

size_t SocketImpl::SendTo(char const *data, size_t size, SockAddrView const &dstAddr)
{
  constexpr int flags = 0;
  auto sent = ::sendto(fd,
                       data, size,
                       flags,
                       dstAddr.addr, dstAddr.addrLen);
  if(sent < 0) {
    auto error = SocketError(); // cache before risking another
    throw std::system_error(error, "failed to send to " + to_string(dstAddr));
  } else if(static_cast<size_t>(sent) != size) {
    throw std::logic_error("unexpected UDP send result");
  }
  return static_cast<size_t>(sent);
}

void SocketImpl::Connect(SockAddrView const &connectAddr)
{
  if(::connect(fd, connectAddr.addr, connectAddr.addrLen)) {
    auto error = SocketError(); // cache before risking another
    throw std::system_error(error, "failed to connect to " + to_string(connectAddr));
  }
}

void SocketImpl::Bind(SockAddrView const &bindAddr)
{
  if(::bind(fd, bindAddr.addr, bindAddr.addrLen)) {
    auto error = SocketError(); // cache before risking another
    throw std::system_error(error, "failed to bind socket to address " + to_string(bindAddr));
  }
}

void SocketImpl::Listen()
{
  constexpr int backlog = 128;
  if(::listen(fd, backlog)) {
    throw std::system_error(SocketError(), "failed to listen");
  }
}

std::optional<std::pair<SocketTcp, Address>>
SocketImpl::Accept(Duration timeout)
{
  if(!WaitReadable(fd, timeout)) {
    return {std::nullopt}; // timeout exceeded
  }
  return Accept();
}

std::pair<SocketTcp, Address> SocketImpl::Accept()
{
  auto [clientFd, clientAddr] = sockpuppet::Accept(fd);
  return {
    SocketTcp(std::make_unique<SocketImpl>(clientFd)),
    std::move(clientAddr)
  };
}

void SocketImpl::SetSockOptNonBlocking()
{
  SetBlocking(fd, false, "failed to set socket option non-blocking");
}

void SocketImpl::SetSockOptReuseAddr()
{
  DoSetSockOpt(fd, SO_REUSEADDR, 1, "failed to set socket option address reuse");
}

void SocketImpl::SetSockOptBroadcast()
{
  DoSetSockOpt(fd, SO_BROADCAST, 1, "failed to set socket option broadcast");
}

void SocketImpl::SetSockOptNoSigPipe()
{
#ifdef SO_NOSIGPIPE
  // avoid SIGPIPE on connection closed (in OSX)
  DoSetSockOpt(fd, SO_NOSIGPIPE, 1, "failed to set socket option non-SIGPIPE");
#endif // SO_NOSIGPIPE
}

void SocketImpl::SetSockOpt(int level, int id, char const *opt, socklen_t optLen)
{
  DoSetSockOpt(fd, level, id, opt, optLen, "failed to set socket option");
}

size_t SocketImpl::GetSockOptRcvBuf() const
{
  auto size = GetSockOpt<int>(fd, SO_RCVBUF, "failed to get socket receive buffer size");
  if(size < 0) {
    throw std::logic_error("unexpected receive buffer size");
  }
  return static_cast<size_t>(size);
}

std::shared_ptr<SockAddrStorage> SocketImpl::GetSockName() const
{
  auto sas = std::make_shared<SockAddrStorage>();
  if(::getsockname(fd, sas->Addr(), sas->AddrLen())) {
    throw std::system_error(SocketError(), "failed to get socket address");
  }
  return sas;
}

std::shared_ptr<SockAddrStorage> SocketImpl::GetPeerName() const
{
  auto sas = std::make_shared<SockAddrStorage>();
  if(::getpeername(fd, sas->Addr(), sas->AddrLen())) {
    throw std::system_error(SocketError(), "failed to get peer address");
  }
  return sas;
}

void SocketImpl::DriverQuery(short &)
{
  // only actively used by the TLS socket
}

void SocketImpl::DriverPending()
{
  // this interface is intended for the TLS socket only
  assert(false);
}


size_t ReceiveNow(SOCKET fd, char *data, size_t size)
{
  constexpr int flags = 0;
  auto received = ::recv(fd,
                         data, size,
                         flags);
  if(received < 0) {
    throw std::system_error(SocketError(), "failed to receive");
  } else if(received == 0) {
    throw std::runtime_error("connection closed");
  }
  return static_cast<size_t>(received);
}

std::optional<size_t> Receive(SOCKET fd, char *data, size_t size, Duration timeout)
{
  if(!WaitReadable(fd, timeout)) {
    return {std::nullopt}; // timeout exceeded
  }
  return {ReceiveNow(fd, data, size)};
}

size_t SendNow(SOCKET fd, char const *data, size_t size)
{
  auto sent = ::send(fd,
                     data, size,
                     sendFlags);
  if(sent < 0) {
    throw std::system_error(SocketError(), "failed to send");
  } else if((sent == 0) && (size > 0U)) {
    throw std::logic_error("unexpected send result");
  }
  assert(static_cast<size_t>(sent) <= size);
  return static_cast<size_t>(sent);
}

size_t SendAll(SOCKET fd, char const *data, size_t size)
{
  constexpr auto noTimeout = Duration(-1);
  auto remaining = std::string_view(data, size);

  do {
    (void)WaitWritable(fd, noTimeout);
    auto sent = SendNow(fd, remaining.data(), remaining.size());
    remaining.remove_prefix(sent);
  } while(!remaining.empty());

  assert(remaining.empty());
  return size - remaining.size();
}

size_t SendTry(SOCKET fd, char const *data, size_t size)
{
  constexpr auto zeroTimeout = Duration(0);

  if(!WaitWritable(fd, zeroTimeout)) {
    return 0U; // timeout exceeded
  }
  return SendNow(fd, data, size);
}

size_t SendSome(SOCKET fd, char const *data, size_t size, DeadlineLimited &deadline)
{
  auto remaining = std::string_view(data, size);

  do {
    if(!WaitWritable(fd, deadline.Remaining())) {
      break; // timeout exceeded
    }
    deadline.Tick();
    auto sent = SendNow(fd, remaining.data(), remaining.size());
    remaining.remove_prefix(sent);
  } while(!remaining.empty() && deadline.TimeLeft());

  assert(size >= remaining.size());
  return size - remaining.size();
}

std::pair<SOCKET, Address> Accept(SOCKET fd)
{
  auto sas = std::make_shared<SockAddrStorage>();
  auto clientFd = ::accept(fd, sas->Addr(), sas->AddrLen());
  return {
    clientFd,
    Address(std::move(sas))
  };
}

} // namespace sockpuppet
