#include "wait.h"
#include "error_code.h" // for SocketError

#include <cassert> // for assert

namespace sockpuppet {

namespace {

int DoPoll(pollfd *pfds, size_t count, int timeoutMs)
{
#ifdef _WIN32
  return ::WSAPoll(pfds,
                   static_cast<ULONG>(count),
                   timeoutMs);
#else
  return ::poll(pfds,
                static_cast<nfds_t>(count),
                timeoutMs);
#endif // _WIN32
}

int DoPoll(pollfd pfd, int timeoutMs)
{
  return DoPoll(&pfd, 1, timeoutMs);
}

int ToMsec(Duration timeout)
{
  using namespace std::chrono;
  using MilliSeconds = duration<int, std::milli>;

  return duration_cast<MilliSeconds>(timeout).count();
}

bool Wait(SOCKET fd, short events, Duration timeout)
{
  if(auto result = DoPoll(pollfd{fd, events, 0}, ToMsec(timeout))) {
    if(result < 0) {
      throw std::system_error(
          SocketError(),
          (events == POLLIN ?
             "failed to wait for socket readable" :
             "failed to wait for socket writable"));
    }
    return true; // read/write ready
  }
  return false; // timeout exceeded
}

} // unnamed namespace

DeadlineUnlimited::DeadlineUnlimited()
  : now(Clock::now())
{
}

void DeadlineUnlimited::Tick()
{
  now = Clock::now();
}

bool DeadlineUnlimited::TimeLeft() const
{
  return true;
}

Duration DeadlineUnlimited::Remaining() const
{
  return Duration(-1);
}


DeadlineLimited::DeadlineLimited(Duration timeout)
  : DeadlineUnlimited()
  , deadline(this->now + timeout)
{
}

bool DeadlineLimited::TimeLeft() const
{
  return (this->now <= deadline);
}

Duration DeadlineLimited::Remaining() const
{
  auto remaining = std::chrono::duration_cast<Duration>(deadline - this->now);
  if(remaining.count() < 0) {
    return Duration(0); // must not turn timeout >=0 into <0
  }
  return remaining;
}


bool WaitReadableBlocking(SOCKET fd, Duration timeout)
{
  return ((timeout.count() < 0) ||
          WaitReadableNonBlocking(fd, timeout));
}

bool WaitReadableNonBlocking(SOCKET fd, Duration timeout)
{
  return Wait(fd, POLLIN, timeout);
}

bool WaitWritableBlocking(SOCKET fd, Duration timeout)
{
  return ((timeout.count() < 0) ||
          WaitWritableNonBlocking(fd, timeout));
}

bool WaitWritableNonBlocking(SOCKET fd, Duration timeout)
{
  return Wait(fd, POLLOUT, timeout);
}

bool Wait(std::vector<pollfd> &pfds, Duration timeout)
{
  if(auto result = DoPoll(pfds.data(), pfds.size(), ToMsec(timeout))) {
    if(result < 0) {
      throw std::system_error(
          SocketError(),
          "failed to wait for socket readable/writable");
    }
    return true; // one or more readable/writable
  }
  return false; // timeout exceeded
}

} // namespace sockpuppet
