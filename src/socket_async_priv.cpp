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

  struct FdEqual
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

  struct WhenBefore
  {
    TimePoint when;

    bool operator()(ToDoShared const &todo) const
    {
      return (when < todo->when);
    }
  };

  struct IsSame
  {
    ToDo::ToDoPriv *ptr;

    bool operator()(ToDoShared const &todo) const
    {
      return (todo.get() == ptr);
    }
  };

  struct DeadlineUnlimited
  {
    Duration remaining;
    TimePoint now;

    DeadlineUnlimited(Duration timeout)
      : remaining(timeout)
      , now(Clock::now())
    {
    }

    void Tick()
    {
      now = Clock::now();
    }

    bool TimeLeft() const
    {
      return true;
    }

    Duration Remaining() const
    {
      return remaining;
    }

    Duration Remaining(TimePoint until) const
    {
      return std::chrono::duration_cast<Duration>(until - now);
    }
  };

  struct DeadlineLimited : public DeadlineUnlimited
  {
    TimePoint deadline;

    DeadlineLimited(Duration timeout)
      : DeadlineUnlimited(timeout)
      , deadline(now + timeout)
    {
    }

    bool TimeLeft() const
    {
      return (now <= deadline);
    }

    Duration Remaining() const
    {
      return DeadlineUnlimited::Remaining(deadline);
    }

    Duration Remaining(TimePoint until) const
    {
      return DeadlineUnlimited::Remaining(std::min(until, deadline));
    }
  };
} // unnamed namespace

void ToDos::Insert(ToDoShared todo)
{
  auto where = Find(WhenBefore{todo->when});
  (void)emplace(where, std::move(todo));
}

void ToDos::Remove(ToDo::ToDoPriv *todo)
{
  auto it = Find(IsSame{todo});
  if(it != end()) { // may have already been removed
    (void)erase(it);
  }
}

void ToDos::Move(ToDoShared todo, TimePoint when)
{
  Remove(todo.get());

  todo->when = when;
  Insert(std::move(todo));
}

template<typename Pred>
std::deque<ToDoShared>::iterator ToDos::Find(Pred pred)
{
  return std::find_if(begin(), end(), pred);
}


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
    auto remaining = (timeout.count() >= 0 ?
        StepTodos(DeadlineLimited(timeout)) :
        StepTodos(DeadlineUnlimited(timeout)));

    // must not turn timeout >=0 into <0
    assert((timeout.count() < 0) || (remaining.count() >= 0));

    // run sockets with remaining time
    StepFds(remaining);
  }
}

template<typename Deadline>
Duration SocketDriver::SocketDriverPriv::StepTodos(Deadline deadline)
{
  do {
    assert(!todos.empty());
    auto &front = todos.front();

    // check if pending task is due, if not return time until it is
    if(front->when > deadline.now) {
      return deadline.Remaining(front->when);
    }

    // take task from list and execute it
    auto task = std::move(front);
    todos.pop_front();
    task->what(); // user task may invalidate todo iterators/references

    // check if pending tasks or time remain
    deadline.Tick();
    if(todos.empty()) {
      return deadline.Remaining();
    }
  } while(deadline.TimeLeft());
  return Duration(0);
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

void SocketDriver::SocketDriverPriv::ToDoInsert(ToDoShared todo)
{
  PauseGuard lock(*this);
  todos.Insert(std::move(todo));
}

void SocketDriver::SocketDriverPriv::ToDoRemove(ToDo::ToDoPriv *todo)
{
  PauseGuard lock(*this);
  todos.Remove(todo);
}

void SocketDriver::SocketDriverPriv::ToDoMove(ToDoShared todo, TimePoint when)
{
  PauseGuard lock(*this);
  todos.Move(std::move(todo), when);
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
        FdEqual{fd});
  assert(itSocket != std::end(sockets));
  sockets.erase(itSocket);

  auto const itPfd = std::find_if(
        std::begin(pfds),
        std::end(pfds),
        FdEqual{fd});
  assert(itPfd != std::end(pfds));
  pfds.erase(itPfd);
}

void SocketDriver::SocketDriverPriv::AsyncWantSend(SOCKET fd)
{
  PauseGuard lock(*this);

  auto const itPfd = std::find_if(
        std::begin(pfds),
        std::end(pfds),
        FdEqual{fd});
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


ToDo::ToDoPriv::ToDoPriv(DriverShared &driver, std::function<void()> what)
  : driver(driver)
  , what(std::move(what))
{
}

ToDo::ToDoPriv::ToDoPriv(DriverShared &driver, std::function<void()> what, TimePoint when)
  : driver(driver)
  , what(std::move(what))
  , when(when)
{
}

ToDo::ToDoPriv::~ToDoPriv() = default;

void ToDo::ToDoPriv::Cancel()
{
  if(auto const ptr = driver.lock()) {
    ptr->ToDoRemove(this);
  }
}

void ToDo::ToDoPriv::Shift(TimePoint when)
{
  if(auto const ptr = driver.lock()) {
    ptr->ToDoMove(shared_from_this(), when);
  }
}

SocketAsyncPriv::SocketAsyncPriv(SocketPriv &&sock, DriverShared &driver, Handlers handlers)
  : SocketAsyncPriv(SocketBufferedPriv(std::move(sock), 0U, 0U),
                    driver,
                    std::move(handlers))
{
}

SocketAsyncPriv::SocketAsyncPriv(SocketBufferedPriv &&buff, DriverShared &driver, Handlers handlers)
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

std::future<void> SocketAsyncPriv::SendTo(BufferPtr &&buffer, AddressShared dstAddr)
{
  return DoSend(sendToQ, std::move(buffer), std::move(dstAddr));
}

template<typename Queue, typename... Args>
std::future<void> SocketAsyncPriv::DoSend(Queue &q, Args&&... args)
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
