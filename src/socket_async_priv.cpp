#include "socket_async_priv.h"
#include "error_code.h" // for SocketError

#include <algorithm> // for std::find_if
#include <cassert> // for assert

namespace sockpuppet {

namespace {
  auto const noTimeout = Duration(-1);

  int Poll(std::vector<pollfd> &polls, Duration timeout)
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

    bool operator()(SocketAsyncPriv const &async) const
    {
      return (async.fd == fd);
    }

    bool operator()(pollfd const &pfd) const
    {
      return (pfd.fd == fd);
    }
  };

  using Clock = std::chrono::steady_clock;
  using TimePoint = std::chrono::time_point<Clock>;

  struct Deadline
  {
    Duration timeout;
    TimePoint deadline;

    Deadline(TimePoint now,
             Duration timeout)
      : timeout(timeout)
      , deadline(now + timeout)
    {
    }

    bool TimeLeft(TimePoint now) const
    {
      if(timeout.count() >= 0)
        return (now <= deadline);
      return true;
    }

    Duration Remaining(TimePoint now) const
    {
      if(timeout.count() >= 0)
        return Difference(now, deadline);
      return timeout;
    }

    Duration Remaining(TimePoint now, TimePoint until) const
    {
      if(timeout.count() >= 0)
        return Difference(now, std::min(until, deadline));
      return Difference(now, until);
    }

    static Duration Difference(TimePoint now, TimePoint then)
    {
      return std::chrono::duration_cast<Duration>(then - now);
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
  , nextId(0)
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

  if(todos.empty()) {
    StepFds(timeout);
  } else {
    // execute due ToDos while keeping track of the time
    auto remaining = StepTodos(timeout);
    assert((timeout.count() < 0) || (remaining.count() >= 0));

    // run sockets with remaining time
    StepFds(remaining);
  }
}

Duration SocketDriver::SocketDriverPriv::StepTodos(Duration timeout)
{
  auto now = Clock::now();
  Deadline deadline(now, timeout);
  auto todo = std::begin(todos);

  for(; (todo != std::end(todos)) && (todo->when <= now); todo = std::begin(todos)) {
    auto task = std::move(todo->what);
    todos.erase(todo);
    task();

    // check how much time passed and if there is any left
    now = Clock::now();
    if(!deadline.TimeLeft(now))
      return Duration(0);
  }

  if(todo != std::end(todos))
    return deadline.Remaining(now, todo->when);
  return deadline.Remaining(now);
}

void SocketDriver::SocketDriverPriv::StepFds(Duration timeout)
{
  if(auto const result = Poll(pfds, timeout)) {
    if(result < 0) {
      throw std::system_error(SocketError(), "failed to poll");
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
  shouldStop = false;
  while(!shouldStop) {
    Step(noTimeout);
  }
}

void SocketDriver::SocketDriverPriv::Stop()
{
  shouldStop = true;
  Bump();
}

uint64_t SocketDriver::SocketDriverPriv::Schedule(
    Duration delay, std::function<void()> what)
{
  PauseGuard lock(*this);

  auto when = std::chrono::steady_clock::now() + delay;
  auto where = std::find_if(
        std::begin(todos), std::end(todos),
        [&](ToDo const &todo) -> bool {
          return (when < todo.when);
        });
  todos.emplace(where,
      ToDo{
        nextId
      , when
      , std::move(what)
      });

  return nextId++;
}

void SocketDriver::SocketDriverPriv::Unschedule(uint64_t id)
{
  PauseGuard lock(*this);

  todos.erase(std::find_if(
      std::begin(todos), std::end(todos),
      [id](ToDo const &todo) -> bool {
        return (todo.id == id);
      }));
}

void SocketDriver::SocketDriverPriv::Reschedule(uint64_t id, Duration delay)
{
  PauseGuard lock(*this);

  auto from = std::find_if(
        std::begin(todos), std::end(todos),
        [id](ToDo const &todo) -> bool {
          return (todo.id == id);
        });
  if(from == std::end(todos)) {
    throw std::logic_error("");
  }
  auto todo = std::move(*from);
  todos.erase(from);

  auto when = std::chrono::steady_clock::now() + delay;
  todo.when = when;
  auto to = std::find_if(
        std::begin(todos), std::end(todos),
        [when](ToDo const &todo) -> bool {
          return (when < todo.when);
        });
  todos.emplace(to, std::move(todo));
}

void SocketDriver::SocketDriverPriv::AsyncRegister(
    SocketAsyncPriv &sock)
{
  PauseGuard lock(*this);

  sockets.emplace_back(sock);
  pfds.emplace_back(pollfd{sock.fd, POLLIN, 0});
}

void SocketDriver::SocketDriverPriv::AsyncUnregister(SOCKET fd)
{
  PauseGuard lock(*this);

  auto const itSocket = std::find_if(
        std::begin(sockets),
        std::end(sockets),
        CompareFd{fd});
  assert(itSocket != std::end(sockets));
  sockets.erase(itSocket);

  auto const itPfd = std::find_if(
        std::begin(pfds),
        std::end(pfds),
        CompareFd{fd});
  assert(itPfd != std::end(pfds));
  pfds.erase(itPfd);
}

void SocketDriver::SocketDriverPriv::AsyncWantSend(SOCKET fd)
{
  PauseGuard lock(*this);

  auto const itPfd = std::find_if(
        std::begin(pfds),
        std::end(pfds),
        CompareFd{fd});
  assert(itPfd != std::end(pfds));
  itPfd->events |= POLLOUT;
}

void SocketDriver::SocketDriverPriv::Bump()
{
  static char const one = '1';
  auto const sent = pipeFrom.SendTo(&one, sizeof(one),
                                    pipeToAddr->ForUdp(),
                                    noTimeout);
  assert(sent == sizeof(one));
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
      if(sock.DriverDoFdTaskWritable()) {
        pfd.events &= ~POLLOUT;
      }
      return;
    } else if(pfd.revents & (POLLHUP | POLLERR)) {
      sock.DriverDoFdTaskError();
      return;
    }
  }
  throw std::logic_error("unhandled poll event");
}


SocketAsyncPriv::SocketAsyncPriv(SocketPriv &&sock,
    std::shared_ptr<SocketDriver::SocketDriverPriv> &driver, Handlers handlers)
  : SocketAsyncPriv(SocketBufferedPriv(std::move(sock), 0U, 0U),
                    driver,
                    std::move(handlers))
{
}

SocketAsyncPriv::SocketAsyncPriv(SocketBufferedPriv &&buff,
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

SocketAsyncPriv::~SocketAsyncPriv()
{
  if(auto const ptr = driver.lock()) {
    ptr->AsyncUnregister(this->fd);
  }
}

std::future<void> SocketAsyncPriv::Send(BufferPtr &&buffer)
{
  return DoSend(sendQ, std::move(buffer));
}

std::future<void> SocketAsyncPriv::SendTo(BufferPtr &&buffer,
    std::shared_ptr<Address::AddressPriv> dstAddr)
{
  return DoSend(sendToQ, std::move(buffer), std::move(dstAddr));
}

template<typename QueueElement, typename... Args>
std::future<void> SocketAsyncPriv::DoSend(
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

void SocketAsyncPriv::DriverDoFdTaskReadable()
try {
  if(handlers.connect) {
    auto p = this->Accept(noTimeout);
    this->Listen();

    handlers.connect(
          std::move(p.first),
          Address(std::move(p.second)));
  } else if(handlers.receive) {
    handlers.receive(this->Receive(noTimeout));
  } else if(handlers.receiveFrom) {
    auto p = this->ReceiveFrom(noTimeout);
    handlers.receiveFrom(
          std::move(p.first),
          Address(std::move(p.second)));
  } else {
    assert(false);
  }
} catch(std::runtime_error const &) {
  DriverDoFdTaskError();
}

bool SocketAsyncPriv::DriverDoFdTaskWritable()
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
  }
  assert(false); // queue emptied unexpectedly
  return true;
}

void SocketAsyncPriv::DriverDoSend(SendQElement &t)
{
  auto &&promise = std::get<0>(t);
  try {
    auto &&buffer = std::get<1>(t);
    auto const sent = SocketPriv::Send(buffer->data(), buffer->size(), noTimeout);
    // as unlimited timeout is set, partially sent data is not expected
    assert(sent == buffer->size());
    promise.set_value();
  } catch(std::exception const &e) {
    promise.set_exception(std::make_exception_ptr(e));
  }
}

void SocketAsyncPriv::DriverDoSendTo(SendToQElement &t)
{
  auto &&promise = std::get<0>(t);
  try {
    auto &&buffer = std::get<1>(t);
    auto &&addr = std::get<2>(t);
    auto const sent = SocketPriv::SendTo(buffer->data(), buffer->size(),
                                         addr->ForUdp(),
                                         noTimeout);
    assert(sent == buffer->size());
    promise.set_value();
  } catch(std::exception const &e) {
    promise.set_exception(std::make_exception_ptr(e));
  }
}

void SocketAsyncPriv::DriverDoFdTaskError()
{
  if(handlers.disconnect) {
    handlers.disconnect(Address(peerAddr));
  } else {
    // silently discard UDP receive errors
  }
}

} // namespace sockpuppet
