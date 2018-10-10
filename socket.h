#ifndef SOCKET_H
#define SOCKET_H

#include "socket_address.h" // for SocketAddress

#include <cstddef> // for size_t

class Socket
{
public:
  Socket(SocketAddress const &address);
  ~Socket();

  void Transmit(char const *data, size_t size);

  void Receive(char *data, size_t size, long timeout);

private:
  int m_fd;
};

#endif // SOCKET_H
