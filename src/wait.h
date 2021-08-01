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

struct DeadlineUnlimited
{
  TimePoint now;

  DeadlineUnlimited();

  void Tick();

  bool TimeLeft() const;

  Duration Remaining() const;
};

struct DeadlineLimited : public DeadlineUnlimited
{
  TimePoint deadline;

  DeadlineLimited(Duration timeout);

  bool TimeLeft() const;

  Duration Remaining() const;
};

// return true if readable/writable or false if timeout exceeded
bool WaitReadableBlocking(SOCKET fd, Duration timeout);
bool WaitReadableNonBlocking(SOCKET fd, Duration timeout);
bool WaitWritableBlocking(SOCKET fd, Duration timeout);
bool WaitWritableNonBlocking(SOCKET fd, Duration timeout);

// readable/writable socket will be marked accordingly
bool Wait(std::vector<pollfd> &polls, Duration timeout);

} // namespace sockpuppet

#endif // SOCKPUPPET_WAIT_H
