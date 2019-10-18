#ifndef SOCKPUPPET_SOCKET_GUARD_H
#define SOCKPUPPET_SOCKET_GUARD_H

namespace sockpuppet {

struct SocketGuard
{
#ifdef _WIN32
  SocketGuard();
  SocketGuard(SocketGuard const &other) = delete;
  SocketGuard(SocketGuard &&other) = delete;
  ~SocketGuard();
  SocketGuard &operator=(SocketGuard const &other) = delete;
  SocketGuard &operator=(SocketGuard &&other) = delete;
#endif // _WIN32
};

} // namespace sockpuppet

#endif // SOCKPUPPET_SOCKET_GUARD_H
