#include "socket_async_priv.h"

#include <algorithm> // for std::find_if
#include <cassert> // for assert
#include <cstring> // for std::strerror
#include <stdexcept> // for std::runtime_error
#include <string> // for std::string

namespace sockpuppet {

namespace {
  auto const noTimeout = Socket::Duration(-1);

  int Poll(std::vector<pollfd> &polls, SocketDriver::Duration timeout)
  {
    using namespace std::chrono;

    auto const timeoutMs = static_cast<int>(duration_cast<milliseconds>(timeout).count());

#ifdef _WIN32
    return ::WSAPoll(polls.data(),
                     static_cast<ULONG>(polls.size()),
                     timeoutMs);
#else
    return ::poll(polls.data(),
                  static_cast<nfds_t>(polls.size()),
                  timeoutMs);
#endif // _WIN32
  }

  struct CompareFd
  {
    SOCKET fd;

    bool operator()(const SocketAsync::SocketAsyncPriv& async) const
    {
      return (async.fd == fd);
    }

    bool operator()(pollfd const &pfd) const
    {
      return (pfd.fd == fd);
    }
  };
} // unnamed namespace

SocketDriver::SocketDriverPriv::StepGuard::StepGuard(SocketDriverPriv &priv)
  : stepLock(priv.stepMtx)
  , pauseLock(priv.pauseMtx, std::defer_lock)
{
  // block until acquiring step mutex, keep locked during life time
  // do not acquire pause mutex yet
}

SocketDriver::SocketDriverPriv::StepGuard::~StepGuard()
{
  // release step mutex
  stepLock.unlock();

  // briefly acquire pause mutex
  // to allow exchanging step mutex with PauseGuard
  pauseLock.lock();
}


SocketDriver::SocketDriverPriv::PauseGuard::PauseGuard(SocketDriverPriv &priv)
  : stepLock(priv.stepMtx, std::defer_lock)
{
  // try to acquire step mutex
  if(!stepLock.try_lock()) {
    // on failure, do a handshake with StepGuard for step mutex
    // using pause mutex and signalling pipe
    std::lock_guard<std::mutex> pauseLock(priv.pauseMtx);
    priv.Bump();
    stepLock.lock();
  }
}

SocketDriver::SocketDriverPriv::PauseGuard::~PauseGuard() = default;


SocketDriver::SocketDriverPriv::SocketDriverPriv()
  : pipeToAddr(std::make_shared<SockAddrInfo>(0U))
  , pipeFrom(pipeToAddr->Family(), SOCK_DGRAM, IPPROTO_UDP)
  , pipeTo(pipeToAddr->Family(), SOCK_DGRAM, IPPROTO_UDP)
  , pfds(1U, pollfd{pipeTo.fd, POLLIN, 0})
  , shouldStop(false)
{
  // bind to system-assigned port number and update address accordingly
  pipeTo.Bind(pipeToAddr->ForUdp());
  pipeToAddr = pipeTo.GetSockName();

  SockAddrInfo pipeFromAddr(0U);
  pipeFrom.Bind(pipeFromAddr.ForUdp());
}

SocketDriver::SocketDriverPriv::~SocketDriverPriv()
{
  Stop();
}

void SocketDriver::SocketDriverPriv::Step(Duration timeout)
{
  StepGuard lock(*this);

  if(auto const result = Poll(pfds, timeout)) {
    if(result < 0) {
      throw std::runtime_error("poll failed: "
                               + std::string(std::strerror(errno)));
    }
  } else {
    // timeout exceeded
    return;
  }

  // one or more sockets is readable/writable
  if(pfds.front().revents & POLLIN) {
    // a readable signalling pipe triggers re-evaluating the sockets
    Unbump();
  } else if(pfds.front().revents != 0) {
    throw std::logic_error("unexpected signalling pipe poll result");
  } else {
    DoOneFdTask();
  }
}

void SocketDriver::SocketDriverPriv::Run()
{
  while(!shouldStop) {
    Step(noTimeout);
  }
  shouldStop = false;
}

void SocketDriver::SocketDriverPriv::Stop()
{
  shouldStop = true;
  Bump();
}

void SocketDriver::SocketDriverPriv::AsyncRegister(
    SocketAsync::SocketAsyncPriv &sock)
{
  PauseGuard lock(*this);

  sockets.emplace_back(sock);
  pfds.emplace_back(pollfd{sock.fd, POLLIN, 0});
}

void SocketDriver::SocketDriverPriv::AsyncUnregister(SOCKET fd)
{
  PauseGuard lock(*this);

  const auto itSocket = std::find_if(
        std::begin(sockets),
        std::end(sockets),
        CompareFd{fd});
  assert(itSocket != std::end(sockets));
  sockets.erase(itSocket);

  const auto itPfd = std::find_if(
        std::begin(pfds),
        std::end(pfds),
        CompareFd{fd});
  assert(itPfd != std::end(pfds));
  pfds.erase(itPfd);
}

void SocketDriver::SocketDriverPriv::AsyncWantSend(SOCKET fd)
{
  PauseGuard lock(*this);

  const auto itPfd = std::find_if(
        std::begin(pfds),
        std::end(pfds),
        CompareFd{fd});
  assert(itPfd != std::end(pfds));
  itPfd->events |= POLLOUT;
}

void SocketDriver::SocketDriverPriv::Bump()
{
  static char const one = '1';
  pipeFrom.SendTo(&one, sizeof(one), pipeToAddr->ForUdp());
}

void SocketDriver::SocketDriverPriv::Unbump()
{
  char dump[256U];
  (void)pipeTo.Receive(dump, sizeof(dump), noTimeout);
}

void SocketDriver::SocketDriverPriv::DoOneFdTask()
{
  assert(sockets.size() + 1U == pfds.size());

  // user task may unregister/destroy a socket -> handle only one
  for(size_t i = 0U; i < sockets.size(); ++i) {
    auto &&pfd = pfds[i + 1U];
    auto &&sock = sockets[i].get();
    assert(pfd.fd == sock.fd);

    if(pfd.revents & POLLIN) {
      sock.DriverDoFdTaskReadable();
      return;
    } else if(pfd.revents & POLLOUT) {
      if (sock.DriverDoFdTaskWritable()) {
        pfd.events &= ~POLLOUT;
      }
      return;
    } else if(pfd.revents & (POLLHUP | POLLERR)) {
      sock.DriverDoFdTaskError();
      return;
    }
  }

  // TODO are spurious wakeups to be expected?
  throw std::logic_error("unhandled poll event");
}


SocketAsync::SocketAsyncPriv::SocketAsyncPriv(SocketPriv &&sock,
    std::shared_ptr<SocketDriver::SocketDriverPriv> &driver, Handlers handlers)
  : SocketAsyncPriv(SocketBufferedPriv(std::move(sock), 0U, 0U),
                    driver,
                    std::move(handlers))
{
}

SocketAsync::SocketAsyncPriv::SocketAsyncPriv(SocketBufferedPriv &&buff,
    std::shared_ptr<SocketDriver::SocketDriverPriv> &driver, Handlers handlers)
  : SocketBufferedPriv(std::move(buff))
  , driver(driver)
  , handlers(std::move(handlers))
{
  driver->AsyncRegister(*this);

  if(this->handlers.disconnect) {
    // cache remote address as it will be unavailable after disconnect
    peerAddr = this->GetPeerName();
  }
}

SocketAsync::SocketAsyncPriv::~SocketAsyncPriv()
{
  if(auto const ptr = driver.lock()) {
    ptr->AsyncUnregister(this->fd);
  }
}

std::future<void> SocketAsync::SocketAsyncPriv::Send(SocketBufferPtr &&buffer)
{
  return DoSend(sendQ, std::move(buffer));
}

std::future<void> SocketAsync::SocketAsyncPriv::SendTo(
    SocketBufferPtr &&buffer, SocketAddress const &dstAddr)
{
  return DoSend(sendToQ, std::move(buffer), dstAddr);
}

template<typename QueueElement, typename... Args>
std::future<void> SocketAsync::SocketAsyncPriv::DoSend(
    std::queue<QueueElement> &q, Args&&... args)
{
  std::promise<void> promise;
  auto ret = promise.get_future();

  bool wasEmpty;
  {
    std::lock_guard<std::mutex> lock(sendQMtx);

    wasEmpty = q.empty();
    q.emplace(std::move(promise),
              std::forward<Args>(args)...);
  }

  if(wasEmpty) {
    if(auto const ptr = driver.lock()) {
      ptr->AsyncWantSend(this->fd);
    }
  }

  return ret;
}

void SocketAsync::SocketAsyncPriv::DriverDoFdTaskReadable()
try {
  if(handlers.connect) {
    auto t = this->Accept(noTimeout);
    this->Listen();

    handlers.connect(
      std::tuple<SocketTcpClient, SocketAddress>{
        SocketTcpClient(std::move(std::get<0>(t)))
      , SocketAddress(std::move(std::get<1>(t)))
      });
  } else if(handlers.receive) {
    handlers.receive(this->Receive(noTimeout));
  } else if(handlers.receiveFrom) {
    handlers.receiveFrom(this->ReceiveFrom(noTimeout));
  } else {
    assert(false);
  }
} catch(std::runtime_error const &) {
  DriverDoFdTaskError();
}

bool SocketAsync::SocketAsyncPriv::DriverDoFdTaskWritable()
{
  // hold the lock during send/sendto
  // as we already checked that the socket will not block and
  // otherwise we would need to re-lock afterwards to verify that
  // the previously empty queue has not been refilled asynchronously
  std::lock_guard<std::mutex> lock(sendQMtx);

  // socket uses either send or sendto but not both
  assert(sendQ.empty() || sendToQ.empty());

  if(auto const sendQSize = sendQ.size()) {
    DriverDoSend(sendQ.front());
    sendQ.pop();
    return (sendQSize == 1U);
  } else if(auto const sendToQSize = sendToQ.size()) {
    DriverDoSendTo(sendToQ.front());
    sendToQ.pop();
    return (sendToQSize == 1U);
  } else {
    assert(false);
    return true;
  }
}

void SocketAsync::SocketAsyncPriv::DriverDoSend(SendQElement &t)
{
  auto &&promise = std::get<0>(t);
  try {
    auto &&buffer = std::get<1>(t);
    SocketPriv::Send(buffer->data(), buffer->size(), noTimeout);
    promise.set_value();
  } catch(std::exception const &e) {
    promise.set_exception(std::make_exception_ptr(e));
  }
}

void SocketAsync::SocketAsyncPriv::DriverDoSendTo(SendToQElement &t)
{
  auto &&promise = std::get<0>(t);
  try {
    auto &&buffer = std::get<1>(t);
    auto &&addr = std::get<2>(t);
    SocketPriv::SendTo(buffer->data(), buffer->size(),
                       addr.Priv().ForUdp());
    promise.set_value();
  } catch(std::exception const &e) {
    promise.set_exception(std::make_exception_ptr(e));
  }
}

void SocketAsync::SocketAsyncPriv::DriverDoFdTaskError()
{
  if(handlers.disconnect) {
    handlers.disconnect(SocketAddress(peerAddr));
  } else {
    // silently discard UDP receive errors
  }
}

} // namespace sockpuppet
