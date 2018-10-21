#ifndef SOCKET_GUARD_H
#define SOCKET_GUARD_H

#include <mutex> // for std::mutex

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

#endif // SOCKET_GUARD_H