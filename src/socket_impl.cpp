#include "socket_impl.h"
#include "error_code.h" // for SocketError

#ifndef _WIN32
# include <fcntl.h> // for ::fcntl
# include <sys/socket.h> // for ::socket
# include <unistd.h> // for ::close
#endif // _WIN32

#include <algorithm> // for std::remove_if
#include <cassert> // for assert
#include <numeric> // for std::accumulate

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

size_t DoReceiveFrom(SOCKET fd, char *data, size_t size, SockAddrStorage &sas)
{
  constexpr int flags =
#ifdef _WIN32
    0;
#else
    MSG_TRUNC; // return received >size so we can determine truncation
#endif // _WIN32
  auto received = ::recvfrom(
    fd,
    data, size,
    flags,
    sas.Addr(), sas.AddrLen());
  if(received < 0) {
    throw std::system_error(SocketError(), "failed to receive");
  }
#ifndef _WIN32
  if(received > size) {
    throw std::runtime_error("truncated receive");
  }
#endif // _WIN32
  return static_cast<size_t>(received);
}

size_t DoSendTo(SOCKET fd, Views &bufs, SockAddrView const &dstAddr)
{
  constexpr int flags = 0;
#ifdef _WIN32
  DWORD sent;
  auto res = ::WSASendTo(
    fd,
    bufs.data(), bufs.size(),
    &sent,
    flags,
    dstAddr.addr, dstAddr.addrLen,
    nullptr,
    nullptr);
  if(res != 0) {
#else // _WIN32
  msghdr msg = {
    const_cast<sockaddr *>(dstAddr.addr), dstAddr.addrLen,
    bufs.data(), bufs.size(),
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

View::View(const BufferPtr &buffer)
  : View(buffer->data(), buffer->size())
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

Views::Views(const std::vector<BufferPtr> &buffers)
  : ViewsBackend(std::begin(buffers), std::end(buffers))
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
  auto sas = std::make_shared<SockAddrStorage>();
  auto received = DoReceiveFrom(fd, data, size, *sas);
  return {received, Address(std::move(sas))};
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
