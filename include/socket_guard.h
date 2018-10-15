#ifndef SOCKET_GUARD_H
#define SOCKET_GUARD_H

#include <mutex> // for std::mutex

class SocketGuard
{
public:
  SocketGuard();
  ~SocketGuard();

private:
  static unsigned int m_instanceCount;
  static std::mutex m_mtx;
};

#endif // SOCKET_GUARD_H
