#include "socket_async_priv.h"

#include <algorithm> // for std::transform
#include <cassert> // for assert
#include <cstring> // for std::strerror
#include <stdexcept> // for std::runtime_error
#include <string> // for std::string

namespace sockpuppet {

namespace {
  int Poll(std::vector<pollfd> &polls, int timeoutMs)
  {
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
} // unnamed namespace

SocketDriver::SocketDriverPriv::StepGuard::StepGuard(SocketDriverPriv &priv)
  : stepLock(priv.stepMtx)
  , pauseLock(priv.pauseMtx, std::defer_lock)
{
}

SocketDriver::SocketDriverPriv::StepGuard::~StepGuard()
{
  stepLock.unlock();
  pauseLock.lock();
}


SocketDriver::SocketDriverPriv::PauseGuard::PauseGuard(SocketDriverPriv &priv)
  : stepLock(priv.stepMtx, std::defer_lock)
  , pauseLock(priv.pauseMtx, std::defer_lock)
{
  if(!stepLock.try_lock()) {
    pauseLock.lock();
    priv.Bump();
    stepLock.lock();
  }
}

SocketDriver::SocketDriverPriv::PauseGuard::~PauseGuard() = default;


SocketDriver::SocketDriverPriv::SocketDriverPriv()
  : pipeToAddr(std::make_shared<SockAddrInfo>(0))
  , pipeFrom(pipeToAddr->Family(), SOCK_DGRAM, IPPROTO_UDP)
  , pipeTo(pipeToAddr->Family(), SOCK_DGRAM, IPPROTO_UDP)
  , shouldStop(false)
{
  // bind to system-assigned port number and update address accordingly
  pipeTo.Bind(pipeToAddr->ForUdp());
  pipeToAddr = pipeTo.GetSockName();

  SockAddrInfo pipeFromAddr(0);
  pipeFrom.Bind(pipeFromAddr.ForUdp());
}

SocketDriver::SocketDriverPriv::~SocketDriverPriv()
{
  Stop();
}

void SocketDriver::SocketDriverPriv::Step(SocketBuffered::Time timeout)
{
  using namespace std::chrono;

  if(timeout.count() > 0U) {
    Step(static_cast<int>(duration_cast<milliseconds>(timeout).count()));
  } else {
    Step();
  }
}

void SocketDriver::SocketDriverPriv::Step(int timeoutMs)
{
  StepGuard guard(*this);

  auto pfds = PrepareFds();

  if(auto const result = Poll(pfds, timeoutMs)) {
    if(result < 0) {
      throw std::runtime_error("poll failed: "
                               + std::string(std::strerror(errno)));
    }
  } else {
    // timeout exceeded
    return;
  }

  // one or more sockets is readable/writable
  if(pfds.back().revents & POLLIN) {
    // a readable signalling socket triggers re-evaluating the sockets
    Unbump();
  } else if(pfds.back().revents != 0) {
    throw std::logic_error("unexpected signalling socket poll result");
  } else {
    pfds.pop_back();
    DoOneFdTask(pfds);
  }
}

void SocketDriver::SocketDriverPriv::Run()
{
  while(!shouldStop) {
    Step();
  }
  shouldStop = false;
}

void SocketDriver::SocketDriverPriv::Stop()
{
  shouldStop = true;
  Bump();
}

void SocketDriver::SocketDriverPriv::Register(
    SocketAsync::SocketAsyncPriv &sock)
{
  PauseGuard guard(*this);
  sockets.emplace_back(sock);
}

void SocketDriver::SocketDriverPriv::Unregister(
    SocketAsync::SocketAsyncPriv &sock)
{
  PauseGuard guard(*this);
  sockets.erase(
    std::remove_if(
      std::begin(sockets),
      std::end(sockets),
      [&](SocketRef s) -> bool {
        return (&s.get() == &sock);
      }),
    std::end(sockets));
}

void SocketDriver::SocketDriverPriv::Bump()
{
  static char const one = '1';
  pipeFrom.SendTo(&one, sizeof(one), pipeToAddr->ForUdp());
}

void SocketDriver::SocketDriverPriv::Unbump()
{
  char dump[256U];
  (void)pipeTo.Receive(dump, sizeof(dump), Socket::Time(0));
}

std::vector<pollfd> SocketDriver::SocketDriverPriv::PrepareFds()
{
  std::vector<pollfd> ret;
  ret.reserve(sockets.size() + 1U);

  // have the sockets set their file descriptors
  std::transform(
        std::begin(sockets),
        std::end(sockets),
        std::back_inserter(ret),
        [](decltype(sockets)::value_type sock) -> pollfd {
    return sock.get().DriverPrepareFd();
  });

  // set the signalling socket file descriptor
  ret.emplace_back(pollfd{pipeTo.fd, POLLIN, 0});

  return ret;
}

void SocketDriver::SocketDriverPriv::DoOneFdTask(std::vector<pollfd> const &pfds)
{
  assert(sockets.size() == pfds.size());

  for(size_t i = 0U; i < sockets.size(); ++i) {
    if(sockets[i].get().DriverDoFdTask(pfds[i])) {
      // user task may unregister/destroy a socket -> handle only one
      break;
    }
  }
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
  , handlers(handlers)
{
  driver->Register(*this);

  if(handlers.disconnect) {
    // cache remote address as it will be unavailable after disconnect
    peerAddr = this->GetPeerName();
  }
}

SocketAsync::SocketAsyncPriv::~SocketAsyncPriv()
{
  if(auto const ptr = driver.lock()) {
    ptr->Unregister(*this);
  }
}

std::future<void> SocketAsync::SocketAsyncPriv::Send(SocketBufferPtr &&buffer)
{
  return DoSend(sendQ, std::move(buffer));
}

std::future<void> SocketAsync::SocketAsyncPriv::SendTo(
    SocketBufferPtr &&buffer, SockAddrView const &dstAddr)
{
  return DoSend(sendToQ, std::move(buffer), dstAddr);
}

template<typename QueueElement, typename... Args>
std::future<void> SocketAsync::SocketAsyncPriv::DoSend(
    std::queue<QueueElement> &q, Args&&... args)
{
  std::promise<void> promise;
  auto ret = promise.get_future();

  {
    std::lock_guard<std::mutex> lock(sendQMtx);

    q.emplace(std::move(promise),
              std::forward<Args>(args)...);
  }

  if(auto const ptr = driver.lock()) {
    ptr->Bump();
  }

  return ret;
}

pollfd SocketAsync::SocketAsyncPriv::DriverPrepareFd()
{
  pollfd pfd = {this->fd, 0, 0};

  if(handlers.connect || handlers.receive || handlers.receiveFrom) {
    pfd.events |= POLLIN;
  }

  {
    std::lock_guard<std::mutex> lock(sendQMtx);

    if(!sendQ.empty() || !sendToQ.empty()) {
      pfd.events |= POLLOUT;
    }
  }

  return pfd;
}

bool SocketAsync::SocketAsyncPriv::DriverDoFdTask(pollfd const &pfd)
{
  assert(pfd.fd == this->fd);

  if(pfd.revents & POLLIN) {
    DriverDoFdTaskReadable();
    return true;
  } else if(pfd.revents & POLLOUT) {
    DriverDoFdTaskWritable();
    return true;
  } else if(pfd.revents & (POLLHUP | POLLERR)) {
    DriverDoFdTaskError();
    return true;
  }
  return false;
}

void SocketAsync::SocketAsyncPriv::DriverDoFdTaskReadable()
try {
  if(handlers.connect) {
    auto t = this->Accept(Time(0));
    this->Listen();

    handlers.connect(
      std::tuple<SocketTcpClient, SocketAddress>{
        SocketTcpClient(std::move(std::get<0>(t)))
      , SocketAddress(std::move(std::get<1>(t)))
      });
  } else if(handlers.receive) {
    handlers.receive(this->Receive(Time(0)));
  } else if(handlers.receiveFrom) {
    handlers.receiveFrom(this->ReceiveFrom(Time(0)));
  } else {
    throw std::logic_error("no read handler");
  }
} catch(std::runtime_error const &) {
  DriverDoFdTaskError();
}

void SocketAsync::SocketAsyncPriv::DriverDoFdTaskWritable()
{
  std::unique_lock<std::mutex> lock(sendQMtx);

  if(!sendQ.empty()) {
    auto e = std::move(sendQ.front());
    sendQ.pop();
    lock.unlock();

    auto &&promise = std::get<0>(e);
    auto &&buffer = std::get<1>(e);

    try {
      SocketPriv::Send(buffer->data(), buffer->size(), Time(0));
      promise.set_value();
    } catch(std::exception const &e) {
      promise.set_exception(std::make_exception_ptr(e));
    }
  } else if(!sendToQ.empty()) {
    auto e = std::move(sendToQ.front());
    sendToQ.pop();
    lock.unlock();

    auto &&promise = std::get<0>(e);
    auto &&buffer = std::get<1>(e);
    auto &&addr = std::get<2>(e);

    try {
      SocketPriv::SendTo(buffer->data(), buffer->size(), addr);
      promise.set_value();
    } catch(std::exception const &e) {
      promise.set_exception(std::make_exception_ptr(e));
    }
  } else {
    throw std::logic_error("send buffer emptied unexpectedly");
  }
}

void SocketAsync::SocketAsyncPriv::DriverDoFdTaskError()
{
  if(handlers.disconnect) {
    handlers.disconnect(SocketAddress(peerAddr));
  }
}

} // namespace sockpuppet
