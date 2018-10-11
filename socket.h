#ifndef SOCKET_H
#define SOCKET_H

#include "socket_address.h" // for SocketAddress
#include "socket_guard.h" // for SocketGuard

#include <cstddef> // for size_t

class Socket
{
public:
  Socket(SocketAddress const &bindAddress);
  ~Socket();

  void Transmit(char const *data,
                size_t size,
                SocketAddress const &dstAddress);

  size_t Receive(char *data,
                 size_t size);

private:
  SocketGuard m_socketGuard;  ///< Guard to initialize socket subsystem on windows
  int m_fd;  ///< Socket file descriptor
};

#endif // SOCKET_H
