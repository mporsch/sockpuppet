#include "driver_impl.h"
#include "address_impl.h" // for Address::AddressImpl
#include "socket_async_impl.h" // for SocketAsyncImpl
#include "wait.h" // for DeadlineLimited

#include <algorithm> // for std::find_if
#include <cassert> // for assert

namespace sockpuppet {

namespace {

constexpr auto noTimeout = Duration(-1);

struct FdEqual
{
  SOCKET fd;

  bool operator()(SocketAsyncImpl const &async) const
  {
    return (async.buff->sock->fd == fd);
  }

  bool operator()(pollfd const &pfd) const
  {
    return (pfd.fd == fd);
  }
};

template<typename Rep, typename Period>
Duration MinDuration(
    std::chrono::duration<Rep, Period> const &lhs,
    Duration const &rhs)
{
  if(rhs.count() < 0) {
    return std::chrono::duration_cast<Duration>(lhs);
  }
  return std::min(std::chrono::duration_cast<Duration>(lhs), rhs);
}

} // unnamed namespace

Driver::DriverImpl::StepGuard::StepGuard(DriverImpl &impl)
  : stepLock(impl.stepMtx)
  , pauseLock(impl.pauseMtx, std::defer_lock)
{
  // block until acquiring step mutex, keep locked during life time
  // do not acquire pause mutex yet
}

Driver::DriverImpl::StepGuard::~StepGuard()
{
  // release step mutex
  stepLock.unlock();

  // briefly acquire pause mutex
  // to allow exchanging step mutex with PauseGuard
  pauseLock.lock();
}


Driver::DriverImpl::PauseGuard::PauseGuard(DriverImpl &impl)
  : stepLock(impl.stepMtx, std::defer_lock)
{
  // try to acquire step mutex
  if(!stepLock.try_lock()) {
    // on failure, do a handshake with StepGuard for step mutex
    // using pause mutex and signalling pipe
    std::lock_guard<std::mutex> pauseLock(impl.pauseMtx);
    impl.Bump();
    stepLock.lock();
  }
}

Driver::DriverImpl::PauseGuard::~PauseGuard() = default;


Driver::DriverImpl::DriverImpl()
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

Driver::DriverImpl::~DriverImpl()
{
  shouldStop = true;

  // block until Step/Run has returned
  PauseGuard lock(*this);
}

void Driver::DriverImpl::Step(Duration timeout)
{
  StepGuard lock(*this);

  if(todos.empty()) {
    StepFds(timeout);
  } else {
    // execute due ToDos while keeping track of the time
    auto remaining =
        (timeout.count() < 0 ?
           StepTodos(DeadlineUnlimitedTime()) :
           (timeout.count() == 0 ?
              StepTodos(DeadlineZeroTime()) :
              StepTodos(DeadlineLimited(timeout))));

    // run sockets with remaining time
    StepFds(remaining);
  }
}

template<typename Deadline>
Duration Driver::DriverImpl::StepTodos(Deadline deadline)
{
  do {
    assert(!todos.empty());
    auto &front = todos.front();

    // check if pending task is due, if not return time until it is
    auto until = front->when - deadline.now;
    if(until.count() > 0) {
      return MinDuration(until, deadline.Remaining());
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

void Driver::DriverImpl::StepFds(Duration timeout)
{
  if(!Wait(pfds, timeout)) {
    return; // timeout exceeded
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

void Driver::DriverImpl::Run()
{
  shouldStop = false;
  while(!shouldStop) {
    Step(noTimeout);
  }
}

void Driver::DriverImpl::Stop()
{
  shouldStop = true;
  Bump();
}

void Driver::DriverImpl::ToDoInsert(ToDoShared todo)
{
  PauseGuard lock(*this);
  todos.Insert(std::move(todo));
}

void Driver::DriverImpl::ToDoRemove(ToDo::ToDoImpl *todo)
{
  PauseGuard lock(*this);
  todos.Remove(todo);
}

void Driver::DriverImpl::ToDoMove(ToDoShared todo, TimePoint when)
{
  PauseGuard lock(*this);
  todos.Move(std::move(todo), when);
}

void Driver::DriverImpl::AsyncRegister(
    SocketAsyncImpl &sock)
{
  PauseGuard lock(*this);

  sockets.emplace_back(sock);
  pfds.emplace_back(pollfd{sock.buff->sock->fd, POLLIN, 0});
}

void Driver::DriverImpl::AsyncUnregister(SOCKET fd)
{
  PauseGuard lock(*this);

  auto itSocket = std::find_if(begin(sockets), end(sockets), FdEqual{fd});
  assert(itSocket != end(sockets));
  sockets.erase(itSocket);

  auto itPfd = std::find_if(begin(pfds), end(pfds), FdEqual{fd});
  assert(itPfd != end(pfds));
  pfds.erase(itPfd);
}

void Driver::DriverImpl::AsyncWantSend(SOCKET fd)
{
  PauseGuard lock(*this);

  auto itPfd = std::find_if(begin(pfds), end(pfds), FdEqual{fd});
  assert(itPfd != end(pfds));
  itPfd->events |= POLLOUT;
}

void Driver::DriverImpl::Bump()
{
  static char const one = '1';
 [[maybe_unused]] auto sent = pipeFrom.SendTo(
        &one, sizeof(one),
        pipeToAddr->ForUdp(),
        noTimeout);
  assert(sent == sizeof(one));
}

void Driver::DriverImpl::Unbump()
{
  char dump[256U];
  (void)pipeTo.ReceiveFrom(dump, sizeof(dump));
}

void Driver::DriverImpl::DoOneFdTask()
{
  assert(sockets.size() + 1U == pfds.size());

  // user task may unregister/destroy a socket -> handle only one
  for(size_t i = 0U; i < sockets.size(); ++i) {
    auto &&pfd = pfds[i + 1U];
    auto &&sock = sockets[i].get();
    assert(pfd.fd == sock.buff->sock->fd);

    if(pfd.revents & POLLIN) {
      sock.DriverOnReadable();
      return;
    } else if(pfd.revents & POLLOUT) {
      if(sock.DriverOnWritable()) {
        pfd.events &= ~POLLOUT;
      }
      return;
    } else if(pfd.revents & (POLLHUP | POLLERR)) {
      sock.DriverOnError("poll hangup/error");
      return;
    }
  }
  throw std::logic_error("unhandled poll event");
}

} // namespace sockpuppet
