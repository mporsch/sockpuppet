#include "socket_impl.h"
#include "error_code.h" // for SocketError

#ifndef _WIN32
# include <fcntl.h> // for ::fcntl
# include <sys/socket.h> // for ::socket
# include <unistd.h> // for ::close
#endif // _WIN32

#include <cassert> // for assert

namespace sockpuppet {

namespace {

constexpr auto fdInvalid =
#ifdef _WIN32
    INVALID_SOCKET;
#else
    SOCKET(-1);
#endif // _WIN32

constexpr int sendAllFlags =
#ifdef MSG_NOSIGNAL
    MSG_NOSIGNAL | // avoid SIGPIPE on connection closed (in Linux)
#endif // MSG_NOSIGNAL
    0;

constexpr int sendSomeFlags =
#ifdef MSG_PARTIAL
    MSG_PARTIAL | // dont block if all cannot be sent at once
#endif // MSG_PARTIAL
#ifdef MSG_DONTWAIT
    MSG_DONTWAIT | // dont block if all cannot be sent at once
#endif // MSG_DONTWAIT
    sendAllFlags;

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

size_t DoReceive(SOCKET fd, char *data, size_t size)
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

size_t DoSend(SOCKET fd, char const *data, size_t size, int flags)
{
  auto sent = ::send(fd,
                     data, size,
                     flags);
  if(sent < 0) {
    throw std::system_error(SocketError(), "failed to send");
  } else if((sent == 0) && (size > 0U)) {
    throw std::logic_error("unexpected send result");
  }
  return static_cast<size_t>(sent);
}

void SetSockOpt(SOCKET fd, int id, int value, char const *errorMessage)
{
  if(::setsockopt(fd, SOL_SOCKET, id,
       reinterpret_cast<char const *>(&value), sizeof(value))) {
    throw std::system_error(SocketError(), errorMessage);
  }
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
  return DoReceive(fd, data, size);
}

// used for UDP only
std::optional<std::pair<size_t, Address>>
SocketImpl::ReceiveFrom(char *data, size_t size, Duration timeout)
{
  if(!WaitReadableBlocking(fd, timeout)) {
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
  return {static_cast<size_t>(received), Address(std::move(sas))};
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
    return sockpuppet::SendSome(fd, data, size);
  }
  DeadlineLimited deadline(timeout);
  return sockpuppet::SendSome(fd, data, size, deadline);
}

size_t SocketImpl::SendSome(char const *data, size_t size)
{
  return sockpuppet::SendSome(fd, data, size);
}

// UDP send will block only rarely,
// if the user enqueues faster than the NIC can send
// causing the OS send buffer to fill up
size_t SocketImpl::SendTo(char const *data, size_t size,
    SockAddrView const &dstAddr, Duration timeout)
{
  if(!WaitWritableBlocking(fd, timeout)) {
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
  if(!WaitReadableBlocking(fd, timeout)) {
    return {std::nullopt}; // timeout exceeded
  }
  return Accept();
}

std::pair<SocketTcp, Address> SocketImpl::Accept()
{
  auto sas = std::make_shared<SockAddrStorage>();
  auto client = ::accept(fd, sas->Addr(), sas->AddrLen());
  return {
    SocketTcp(std::make_unique<SocketImpl>(client)),
    Address(std::move(sas))
  };
}

void SocketImpl::SetSockOptNonBlocking()
{
  SetBlocking(fd, false, "failed to set socket option non-blocking");
}

void SocketImpl::SetSockOptReuseAddr()
{
  SetSockOpt(fd, SO_REUSEADDR, 1, "failed to set socket option address reuse");
}

void SocketImpl::SetSockOptBroadcast()
{
  SetSockOpt(fd, SO_BROADCAST, 1, "failed to set socket option broadcast");
}

void SocketImpl::SetSockOptNoSigPipe()
{
#ifdef SO_NOSIGPIPE
  // avoid SIGPIPE on connection closed (in OSX)
  SetSockOpt(fd, SO_NOSIGPIPE, 1, "failed to set socket option non-SIGPIPE");
#endif // SO_NOSIGPIPE
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


std::optional<size_t> Receive(SOCKET fd, char *data, size_t size, Duration timeout)
{
  if(!WaitReadableBlocking(fd, timeout)) {
    return {std::nullopt}; // timeout exceeded
  }
  return {DoReceive(fd, data, size)};
}

size_t SendAll(SOCKET fd, char const *data, size_t size)
{
  // set flags to block until everything is sent
  auto sent = DoSend(fd, data, size, sendAllFlags);
  assert(sent == size);
  return sent;
}

size_t SendSome(SOCKET fd, char const *data, size_t size)
{
  // set flags to send only what can be sent without blocking
  return DoSend(fd, data, size, sendSomeFlags);
}

size_t SendSome(SOCKET fd, char const *data, size_t size, DeadlineLimited &deadline)
{
  size_t sent = 0U;
  do {
    if(!WaitWritableBlocking(fd, deadline.Remaining())) {
      break; // timeout exceeded
    }
    sent += SendSome(fd, data + sent, size - sent);
    deadline.Tick();
  } while((sent < size) && deadline.TimeLeft());
  return sent;
}

} // namespace sockpuppet
