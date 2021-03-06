#include "driver_priv.h"
#include "address_priv.h" // for Address::AddressPriv
#include "error_code.h" // for SocketError
#include "socket_async_priv.h" // for SocketAsyncPriv

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

Driver::DriverPriv::StepGuard::StepGuard(DriverPriv &priv)
  : stepLock(priv.stepMtx)
  , pauseLock(priv.pauseMtx, std::defer_lock)
{
  // block until acquiring step mutex, keep locked during life time
  // do not acquire pause mutex yet
}

Driver::DriverPriv::StepGuard::~StepGuard()
{
  // release step mutex
  stepLock.unlock();

  // briefly acquire pause mutex
  // to allow exchanging step mutex with PauseGuard
  pauseLock.lock();
}


Driver::DriverPriv::PauseGuard::PauseGuard(DriverPriv &priv)
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

Driver::DriverPriv::PauseGuard::~PauseGuard() = default;


Driver::DriverPriv::DriverPriv()
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

Driver::DriverPriv::~DriverPriv()
{
  shouldStop = true;

  // block until Step/Run has returned
  PauseGuard lock(*this);
}

void Driver::DriverPriv::Step(Duration timeout)
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
Duration Driver::DriverPriv::StepTodos(Deadline deadline)
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

void Driver::DriverPriv::StepFds(Duration timeout)
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

void Driver::DriverPriv::Run()
{
  shouldStop = false;
  while(!shouldStop) {
    Step(noTimeout);
  }
}

void Driver::DriverPriv::Stop()
{
  shouldStop = true;
  Bump();
}

void Driver::DriverPriv::ToDoInsert(ToDoShared todo)
{
  PauseGuard lock(*this);
  todos.Insert(std::move(todo));
}

void Driver::DriverPriv::ToDoRemove(ToDo::ToDoPriv *todo)
{
  PauseGuard lock(*this);
  todos.Remove(todo);
}

void Driver::DriverPriv::ToDoMove(ToDoShared todo, TimePoint when)
{
  PauseGuard lock(*this);
  todos.Move(std::move(todo), when);
}

void Driver::DriverPriv::AsyncRegister(
    SocketAsyncPriv &sock)
{
  PauseGuard lock(*this);

  sockets.emplace_back(sock);
  pfds.emplace_back(pollfd{sock.fd, POLLIN, 0});
}

void Driver::DriverPriv::AsyncUnregister(SOCKET fd)
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

void Driver::DriverPriv::AsyncWantSend(SOCKET fd)
{
  PauseGuard lock(*this);

  auto const itPfd = std::find_if(
        std::begin(pfds),
        std::end(pfds),
        FdEqual{fd});
  assert(itPfd != std::end(pfds));
  itPfd->events |= POLLOUT;
}

void Driver::DriverPriv::Bump()
{
  static char const one = '1';
  auto const sent = pipeFrom.SendTo(&one, sizeof(one),
                                    pipeToAddr->ForUdp(),
                                    noTimeout);
  assert(sent == sizeof(one));
}

void Driver::DriverPriv::Unbump()
{
  char dump[256U];
  (void)pipeTo.Receive(dump, sizeof(dump), noTimeout);
}

void Driver::DriverPriv::DoOneFdTask()
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

} // namespace sockpuppet
