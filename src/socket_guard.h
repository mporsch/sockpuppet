#ifndef SOCKET_GUARD_H
#define SOCKET_GUARD_H

#include <mutex> // for std::mutex

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

private:
  static unsigned int m_instanceCount;
  static std::mutex m_mtx;
};

} // namespace sockpuppet

#endif // SOCKET_GUARD_H
