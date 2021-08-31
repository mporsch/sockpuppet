#ifndef SOCKPUPPET_WAIT_H
#define SOCKPUPPET_WAIT_H

#include "sockpuppet/socket.h" // for Duration
#include "sockpuppet/socket_async.h" // for TimePoint

#ifdef _WIN32
# include <winsock2.h> // for pollfd
#else
# include <poll.h> // for pollfd
using SOCKET = int;
#endif // _WIN32

#include <vector> // for std::vector

namespace sockpuppet {

// return true if readable/writable or false if timeout exceeded
bool WaitReadableBlocking(SOCKET fd, Duration timeout);
bool WaitReadableNonBlocking(SOCKET fd, Duration timeout);
bool WaitReadableNonBlockingNoThrow(SOCKET fd, Duration timeout);
bool WaitWritableBlocking(SOCKET fd, Duration timeout);
bool WaitWritableNonBlocking(SOCKET fd, Duration timeout);
bool WaitWritableNonBlockingNoThrow(SOCKET fd, Duration timeout);

// readable/writable socket will be marked accordingly
bool Wait(std::vector<pollfd> &pfds, Duration timeout);

// different deadline specializations that share a common interface
// suitable for use as templated parameter
struct DeadlineUnlimited
{
  DeadlineUnlimited() = default;

  inline void Tick()
  {
  }

  inline bool TimeLeft() const
  {
    return true;
  }

  inline Duration Remaining() const
  {
    return Duration(-1);
  }
};

struct DeadlineUnlimitedTime : public DeadlineUnlimited
{
  TimePoint now;

  DeadlineUnlimitedTime()
    : now(Clock::now())
  {
  }

  inline void Tick()
  {
    now = Clock::now();
  }
};

struct DeadlineZero : public DeadlineUnlimited
{
  DeadlineZero() = default;

  inline bool TimeLeft() const
  {
    return false;
  }

  inline Duration Remaining() const
  {
    return Duration(0);
  }
};

struct DeadlineZeroTime : public DeadlineZero
{
  TimePoint now;

  DeadlineZeroTime()
    : now(Clock::now())
  {
  }

  inline void Tick()
  {
    now = Clock::now();
  }
};

struct DeadlineLimited : public DeadlineUnlimitedTime
{
  TimePoint deadline;

  DeadlineLimited(Duration timeout)
    : DeadlineUnlimitedTime()
    , deadline(this->now + timeout)
  {
  }

  inline bool TimeLeft() const
  {
    return (this->now < deadline);
  }

  inline Duration Remaining() const
  {
    auto remaining = std::chrono::duration_cast<Duration>(deadline - this->now);
    if(remaining.count() < 0) {
      return Duration(0); // must not turn timeout >=0 into <0
    }
    return remaining;
  }
};

} // namespace sockpuppet

#endif // SOCKPUPPET_WAIT_H
