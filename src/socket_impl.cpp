#include "socket_impl.h"
#include "error_code.h" // for SocketError

#ifndef _WIN32
# include <fcntl.h> // for ::fcntl
# include <sys/socket.h> // for ::socket
# include <unistd.h> // for ::close
#endif // _WIN32

#include <cassert> // for assert
#include <numeric>

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

size_t DoSendTo(SOCKET fd, Views &bufs, SockAddrView const &dstAddr)
{
#ifdef _WIN32
  DWORD sent;
  constexpr int flags = 0;
  auto res = ::WSASendTo(fd,
                         bufs.data(), bufs.size(),
                         &sent,
                         flags,
                         dstAddr.addr, dstAddr.addrLen,
                         nullptr,
                         nullptr);
  if(res != 0) {
#else // _WIN32
  msghdr msg = {
    dstAddr.addr, dstAddr.addrLen,
    buf.data(), buf.size(),
    nullptr, 0U,
    0
  };
  auto sent = ::sendmsg(fd, &msg, flags);
  if(sent < 0) {
#endif // _WIN32
    auto error = SocketError(); // cache before risking another
    throw std::system_error(error, "failed to send to " + to_string(dstAddr));
  }

  bufs.Advance(static_cast<size_t>(sent));
  if(!bufs.empty()) {
    throw std::logic_error("unexpected UDP send result");
  }
  return static_cast<size_t>(sent);
}

size_t DoSend(SOCKET fd, Views &bufs, int flags)
{
#ifdef _WIN32
  DWORD sent;
  auto res = ::WSASend(fd,
                       bufs.data(), static_cast<DWORD>(bufs.size()),
                       &sent,
                       static_cast<DWORD>(flags),
                       nullptr,
                       nullptr);
  if(res != 0) {
#else // _WIN32
  msghdr msg = {
    nullptr, 0U,
    buf.data(), buf.size(),
    nullptr, 0U,
    0
  };
  auto sent = ::sendmsg(fd, &msg, flags);
  if(sent < 0) {
#endif // _WIN32
    throw std::system_error(SocketError(), "failed to send");
  }

  bufs.Advance(static_cast<size_t>(sent));
  if((sent == 0) && (bufs.OverallSize() > 0U)) {
    throw std::logic_error("unexpected send result");
  }
  return static_cast<size_t>(sent);
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

View::View(char const *data, size_t size)
#ifdef _WIN32
  : WSABUF{static_cast<u_long>(size), const_cast<char*>(data)}
#else
  :iovec{const_cast<char*>(data), size}
#endif // _WIN32
{
}

View::View(std::string_view sv)
  : View(sv.data(), sv.size())
{
}

char const *View::Data() const
{
#ifdef _WIN32
  return this->buf;
#else
  return static_cast<char*>(this->iov_base);
#endif // _WIN32
}

size_t View::Size() const
{
#ifdef _WIN32
  return this->len;
#else
  return this->iov_len;
#endif // _WIN32
}

void View::Advance(size_t count)
{
  if(count >= Size()) {
    throw std::logic_error("invalid advance size");
  }
#ifdef _WIN32
  this->buf += count;
  this->len -= count;
#else
  this->iov_base = static_cast<char*>(this->iov_base) + count;
  this->iov_len -= count;
#endif // _WIN32
}


Views::Views(char const *data, size_t size)
  : ViewsBackend(1U, View(data, size))
{
}

Views::Views(std::initializer_list<std::string_view> ilist)
  : ViewsBackend(std::begin(ilist), std::end(ilist))
{
}

void Views::Advance(size_t count)
{
  assert(count <= OverallSize());
  this->erase(
    std::remove_if(
      this->begin(), this->end(),
      [&](ViewsBackend::const_reference buf) -> bool {
        if(count >= buf.Size()) {
          count -= buf.Size();
          return true;
        }
        return false;
      }),
    this->end());
  assert((count == 0U) || (this->size() == 1U));
  if(count > 0U) {
    this->front().Advance(count);
  }
}

size_t Views::OverallSize() const
{
  return std::accumulate(this->begin(), this->end(), size_t(0U),
    [](size_t sum, ViewsBackend::const_reference buf) -> size_t {
      return sum + buf.Size();
    });
}


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
  auto bufs = Views(data, size);
  return Send(bufs, timeout);
}

size_t SocketImpl::Send(Views &bufs, Duration timeout)
{
  if(timeout.count() < 0) {
    return SendAll(fd, bufs);
  }
  if(timeout.count() == 0) {
    return SendTry(fd, bufs);
  }
  DeadlineLimited deadline(timeout);
  return sockpuppet::SendSome(fd, bufs, deadline);
}

size_t SocketImpl::SendSome(char const *data, size_t size)
{
  auto bufs = Views(data, size);
  return SendSome(bufs);
}

size_t SocketImpl::SendSome(Views &bufs)
{
  return SendNow(fd, bufs);
}

// UDP send will block only rarely,
// if the user enqueues faster than the NIC can send
// causing the OS send buffer to fill up
size_t SocketImpl::SendTo(Views &bufs,
    SockAddrView const &dstAddr, Duration timeout)
{
  if(!WaitWritable(fd, timeout)) {
    return 0U; // timeout exceeded
  }
  return SendTo(bufs, dstAddr);
}

size_t SocketImpl::SendTo(Views &bufs, SockAddrView const &dstAddr)
{
  return DoSendTo(fd, bufs, dstAddr);
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

size_t SendNow(SOCKET fd, Views &bufs)
{
  return DoSend(fd, bufs, sendFlags);
}

size_t SendAll(SOCKET fd, Views &bufs)
{
  constexpr auto noTimeout = Duration(-1);
  size_t sent = 0U;

  do {
    (void)WaitWritable(fd, noTimeout);
    sent += SendNow(fd, bufs);
  } while(!bufs.empty());

  return sent;
}

size_t SendTry(SOCKET fd, Views &bufs)
{
  constexpr auto zeroTimeout = Duration(0);

  if(!WaitWritable(fd, zeroTimeout)) {
    return 0U; // timeout exceeded
  }
  return SendNow(fd, bufs);
}

size_t SendSome(SOCKET fd, Views &bufs, DeadlineLimited &deadline)
{
  size_t sent = 0U;

  do {
    if(!WaitWritable(fd, deadline.Remaining())) {
      break; // timeout exceeded
    }
    deadline.Tick();
    sent += SendNow(fd, bufs);
  } while(!bufs.empty() && deadline.TimeLeft());

  return sent;
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
