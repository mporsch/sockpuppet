#include "wait.h"
#include "error_code.h" // for SocketError

#include <cassert> // for assert

namespace sockpuppet {

namespace {

int DoPoll(pollfd pfd, int timeout)
{
#ifdef _WIN32
  return ::WSAPoll(&pfd, 1U, timeout);
#else
  return ::poll(&pfd, 1U, timeout);
#endif // _WIN32
}

int ToMsec(Duration timeout)
{
  using namespace std::chrono;
  using MilliSeconds = duration<int, std::milli>;

  return duration_cast<MilliSeconds>(timeout).count();
}

bool Poll(SOCKET fd, short events, Duration timeout)
{
  if(auto const result = DoPoll(pollfd{fd, events, 0}, ToMsec(timeout))) {
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

Deadline::Deadline(Duration timeout)
  : remaining(timeout)
{
  lastStart = Clock::now();
}

bool Deadline::WaitWritable(SOCKET fd) const
{
  assert(remaining.count() >= 0);
  return Poll(fd, POLLOUT, remaining);
}

bool Deadline::TimeLeft()
{
  auto const now = Clock::now();

  remaining -= std::chrono::duration_cast<Duration>(now - lastStart);
  lastStart = now;

  return (remaining.count() > 0);
}


DeadlineUnlimited::DeadlineUnlimited(Duration timeout)
  : remaining(timeout)
  , now(Clock::now())
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
  return remaining;
}

Duration DeadlineUnlimited::Remaining(TimePoint until) const
{
  return std::chrono::duration_cast<Duration>(until - now);
}


DeadlineLimited::DeadlineLimited(Duration timeout)
  : DeadlineUnlimited(timeout)
  , deadline(now + timeout)
{
}

bool DeadlineLimited::TimeLeft() const
{
  return (now <= deadline);
}

Duration DeadlineLimited::Remaining() const
{
  return DeadlineUnlimited::Remaining(deadline);
}

Duration DeadlineLimited::Remaining(TimePoint until) const
{
  return DeadlineUnlimited::Remaining(std::min(until, deadline));
}


bool WaitReadableBlocking(SOCKET fd, Duration timeout)
{
  return ((timeout.count() < 0) ||
          WaitReadableNonBlocking(fd, timeout));
}

bool WaitReadableNonBlocking(SOCKET fd, Duration timeout)
{
  return Poll(fd, POLLIN, timeout);
}

bool WaitWritableBlocking(SOCKET fd, Duration timeout)
{
  return ((timeout.count() < 0) ||
          WaitWritableNonBlocking(fd, timeout));
}

bool WaitWritableNonBlocking(SOCKET fd, Duration timeout)
{
  return Poll(fd, POLLOUT, timeout);
}

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

} // namespace sockpuppet
