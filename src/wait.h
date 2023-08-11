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

namespace wait_detail {

struct Unclocked
{
  inline void Tick()
  {
  }
};

struct Clocked
{
  TimePoint now;

  Clocked()
    : now(Clock::now())
  {
  }

  inline void Tick()
  {
    now = Clock::now();
  }
};

struct Unlimited
{
  inline bool TimeLeft() const
  {
    return true;
  }

  inline Duration Remaining() const
  {
    return Duration(-1);
  }
};

struct ZeroLimited
{
  inline bool TimeLeft() const
  {
    return false;
  }

  inline Duration Remaining() const
  {
    return Duration(0);
  }
};

} // namespace wait_detail

// return true if readable/writable or false if timeout exceeded
bool WaitReadable(SOCKET fd, Duration timeout);
bool WaitWritable(SOCKET fd, Duration timeout);

// readable/writable socket will be marked accordingly
bool Wait(std::vector<pollfd> &pfds, Duration timeout);

// different deadline specializations that share a common interface
// suitable for use as templated parameter
struct DeadlineUnlimited
  : public wait_detail::Unclocked
  , public wait_detail::Unlimited
{
};

struct DeadlineUnlimitedTime
  : public wait_detail::Clocked
  , public wait_detail::Unlimited
{
};

struct DeadlineZero
  : public wait_detail::Unclocked
  , public wait_detail::ZeroLimited
{
};

struct DeadlineZeroTime
  : public wait_detail::Clocked
  , public wait_detail::ZeroLimited
{
};

struct DeadlineLimited : public wait_detail::Clocked
{
  TimePoint deadline;

  DeadlineLimited(Duration timeout)
    : wait_detail::Clocked()
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
