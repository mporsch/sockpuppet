#ifndef SOCKPUPPET_SOCKET_GUARD_H
#define SOCKPUPPET_SOCKET_GUARD_H

namespace sockpuppet {

class SocketGuard
{
public:
  SocketGuard();
  SocketGuard(SocketGuard const &other) = delete;
  SocketGuard(SocketGuard &&other) = delete;
  ~SocketGuard();
  SocketGuard &operator=(SocketGuard const &other) = delete;
  SocketGuard &operator=(SocketGuard &&other) = delete;
};

} // namespace sockpuppet

#endif // SOCKPUPPET_SOCKET_GUARD_H
