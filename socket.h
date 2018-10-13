#ifndef SOCKET_H
#define SOCKET_H

#include "socket_address.h" // for SocketAddress
#include "socket_guard.h" // for SocketGuard

#include <cstddef> // for size_t

class Socket
{
public:
  Socket(Socket const &other) = delete;
  Socket(Socket &&other);
  virtual ~Socket();

  Socket &operator=(Socket const &other) = delete;
  Socket &operator=(Socket &&other);

  size_t Receive(char *data,
                 size_t size);

protected:
  Socket(int family, int type, int protocol);
  Socket(int fd);

  void Bind(SocketAddress const &bindAddress);

protected:
  SocketGuard m_socketGuard;  ///< Guard to initialize socket subsystem on windows
  int m_fd;  ///< Socket file descriptor
};

class SocketUdp : public Socket
{
public:
  SocketUdp(SocketAddress const &bindAddress);

  void Transmit(char const *data,
                size_t size,
                SocketAddress const &dstAddress);
};

class SocketTcpClient : protected Socket
{
  friend class SocketTcpServer;

public:
  SocketTcpClient(SocketAddress const &connectAddress);

  void Transmit(char const *data,
                size_t size);

  using Socket::Receive;

private:
  SocketTcpClient(int fd);
};

class SocketTcpServer : protected Socket
{
public:
  SocketTcpServer(SocketAddress const &bindAddress);

  SocketTcpClient Listen();
};

#endif // SOCKET_H
