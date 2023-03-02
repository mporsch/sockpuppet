#include "socket_impl.h"
#include "error_code.h" // for SocketError
#include "wait.h" // for WaitReadableBlocking

#ifndef _WIN32
# include <fcntl.h> // for ::fcntl
# include <sys/socket.h> // for ::socket
# include <unistd.h> // for ::close
#endif // _WIN32

#include <algorithm>
#include <cassert> // for assert
#include <numeric>
#include <string_view>

namespace sockpuppet {

namespace {

static auto const fdInvalid =
#ifdef _WIN32
    INVALID_SOCKET;
#else
    SOCKET(-1);
#endif // _WIN32

static int const sendAllFlags =
#ifdef MSG_NOSIGNAL
    MSG_NOSIGNAL | // avoid SIGPIPE on connection closed (in Linux)
#endif // MSG_NOSIGNAL
    0;

static int const sendSomeFlags =
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

size_t DoSend(SOCKET fd, Views &buf, int flags)
{
#ifdef _WIN32
  DWORD sent;
  auto res = ::WSASend(fd,
                       buf.data(), static_cast<DWORD>(buf.size()),
                       &sent,
                       static_cast<DWORD>(flags),
                       nullptr,
                       nullptr);
  if(res != 0) {
    throw std::system_error(SocketError(res), "failed to send");
  } else if((sent == 0) && (buf.OverallSize() > 0U)) {
    throw std::logic_error("unexpected send result");
  }
#else // _WIN32
  msghdr msg = {
    nullptr, 0U,
    buf.data(), buf.size(),
    nullptr, 0U,
    0
  };
  auto sent = ::sendmsg(fd, &msg, flags);
  if(sent < 0) {
    throw std::system_error(SocketError(), "failed to send");
  } else if((sent == 0) && (buf.OverallSize() > 0U)) {
    throw std::logic_error("unexpected send result");
  }
#endif // _WIN32

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

View::View(char const *data, size_t size)
#ifdef _WIN32
  : WSABUF{static_cast<u_long>(size), const_cast<char*>(data)}
#else
  :iovec{const_cast<char*>(data), size}
#endif // _WIN32
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
#ifdef _WIN32
  assert(count < this->len);
  this->buf += count;
  this->len -= count;
#else
  assert(count < this->iov_len);
  this->iov_base = static_cast<char*>(this->iov_base) + count;
  this->iov_len -= count;
#endif // _WIN32
}


Views::Views(char const *data, size_t size)
  : ViewsBackend(1U, View(data, size))
{
}

Views::Views(std::initializer_list<std::string_view> ilist)
{
  auto count = std::distance(std::begin(ilist), std::end(ilist));
  this->reserve(static_cast<size_t>(count));
  (void)std::transform(
        std::begin(ilist), std::end(ilist),
        std::back_inserter(*this),
        [](std::string_view str) -> View {
          return View(str.data(), str.size());
        });
}

void Views::Advance(size_t count)
{
  if(count > OverallSize()) {
    throw std::logic_error("invalid advance size");
  }

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
  assert(count < OverallSize());
  if(count > 0) {
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
  if(!WaitReadable(timeout)) {
    return {std::nullopt}; // timeout exceeded
  }
  return {Receive(data, size)};
}

size_t SocketImpl::Receive(char *data, size_t size)
{
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
std::optional<std::pair<size_t, Address>>
SocketImpl::ReceiveFrom(char *data, size_t size, Duration timeout)
{
  if(!WaitReadable(timeout)) {
    return {std::nullopt}; // timeout exceeded
  }
  return {ReceiveFrom(data, size)};
}

std::pair<size_t, Address>
SocketImpl::ReceiveFrom(char *data, size_t size)
{
  static int const flags = 0;
  auto sas = std::make_shared<SockAddrStorage>();
  auto const received = ::recvfrom(fd,
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
size_t SocketImpl::Send(Views &buf, Duration timeout)
{
  return (timeout.count() < 0 ?
            SendAll(buf) :
            (timeout.count() == 0 ?
               SendSome(buf, DeadlineZero()) :
               SendSome(buf, DeadlineLimited(timeout))));
}

size_t SocketImpl::SendAll(Views &buf)
{
  // set flags to block until everything is sent
  auto const sent = DoSend(fd, buf, sendAllFlags);
  assert(sent == size);
  return sent;
}

template<typename Deadline>
size_t SocketImpl::SendSome(Views &buf, Deadline deadline)
{
  size_t sentOverall = 0U;
  do {
    if(!WaitWritable(deadline.Remaining())) {
      break; // timeout exceeded
    }
    auto sent = SendSome(buf);
    buf.Advance(sent);
    sentOverall += sent;
    deadline.Tick();
  } while(!buf.empty() && deadline.TimeLeft());
  return sentOverall;
}

size_t SocketImpl::SendSome(Views &buf)
{
  // set flags to send only what can be sent without blocking
  return DoSend(fd, buf, sendSomeFlags);
}

// UDP send will block only rarely,
// if the user enqueues faster than the NIC can send
// causing the OS send buffer to fill up
size_t SocketImpl::SendTo(char const *data, size_t size,
    SockAddrView const &dstAddr, Duration timeout)
{
  if(!WaitWritable(timeout)) {
    return 0U; // timeout exceeded
  }
  return SendTo(data, size, dstAddr);
}

size_t SocketImpl::SendTo(char const *data, size_t size, SockAddrView const &dstAddr)
{
  static int const flags = 0;
  auto const sent = ::sendto(fd,
                             data, size,
                             flags,
                             dstAddr.addr, dstAddr.addrLen);
  if(sent < 0) {
    auto const error = SocketError(); // cache before risking another
    throw std::system_error(error, "failed to send to " + to_string(dstAddr));
  } else if(static_cast<size_t>(sent) != size) {
    throw std::logic_error("unexpected UDP send result");
  }
  return static_cast<size_t>(sent);
}

void SocketImpl::Connect(SockAddrView const &connectAddr)
{
  if(::connect(fd, connectAddr.addr, connectAddr.addrLen)) {
    auto const error = SocketError(); // cache before risking another
    throw std::system_error(error, "failed to connect to " + to_string(connectAddr));
  }
}

void SocketImpl::Bind(SockAddrView const &bindAddr)
{
  if(::bind(fd, bindAddr.addr, bindAddr.addrLen)) {
    auto const error = SocketError(); // cache before risking another
    throw std::system_error(error, "failed to bind socket to address " + to_string(bindAddr));
  }
}

void SocketImpl::Listen()
{
  static int const backlog = 128;
  if(::listen(fd, backlog)) {
    throw std::system_error(SocketError(), "failed to listen");
  }
}

std::optional<std::pair<SocketTcp, Address>>
SocketImpl::Accept(Duration timeout)
{
  if(!WaitReadable(timeout)) {
    return {std::nullopt}; // timeout exceeded
  }
  return Accept();
}

std::pair<SocketTcp, Address>
SocketImpl::Accept()
{
  auto sas = std::make_shared<SockAddrStorage>();
  auto client = ::accept(fd, sas->Addr(), sas->AddrLen());
  return {
    SocketTcp(std::make_unique<SocketImpl>(client)),
    Address(std::move(sas))
  };
}

bool SocketImpl::WaitReadable(Duration timeout)
{
  return WaitReadableBlocking(fd, timeout);
}

bool SocketImpl::WaitWritable(Duration timeout)
{
  return WaitWritableBlocking(fd, timeout);
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

} // namespace sockpuppet
