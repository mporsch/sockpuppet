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

struct Deadline
{
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;

  Duration remaining;
  TimePoint lastStart;

  Deadline(Duration timeout);

  bool WaitWritable(SOCKET fd) const;

  bool TimeLeft();
};

struct DeadlineUnlimited
{
  Duration remaining;
  TimePoint now;

  DeadlineUnlimited(Duration timeout);

  void Tick();

  bool TimeLeft() const;

  Duration Remaining() const;
  Duration Remaining(TimePoint until) const;
};

struct DeadlineLimited : public DeadlineUnlimited
{
  TimePoint deadline;

  DeadlineLimited(Duration timeout);

  bool TimeLeft() const;

  Duration Remaining() const;
  Duration Remaining(TimePoint until) const;
};

bool WaitReadableBlocking(SOCKET fd, Duration timeout);
bool WaitReadableNonBlocking(SOCKET fd, Duration timeout);
bool WaitWritableBlocking(SOCKET fd, Duration timeout);
bool WaitWritableNonBlocking(SOCKET fd, Duration timeout);

int Poll(std::vector<pollfd> &polls, Duration timeout);

} // namespace sockpuppet

#endif // SOCKPUPPET_WAIT_H
