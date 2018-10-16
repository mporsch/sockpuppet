#ifndef SOCKET_H
#define SOCKET_H

#include "socket_address.h" // for SocketAddress
#include "socket_guard.h" // for SocketGuard

#include <chrono> // for std::chrono
#include <cstddef> // for size_t

class Socket
{
public:
  using Time = std::chrono::duration<uint32_t, std::micro>;

  Socket(Socket const &other) = delete;
  Socket(Socket &&other);
  virtual ~Socket();

  Socket &operator=(Socket const &other) = delete;
  Socket &operator=(Socket &&other);

  /// @return  May return 0 if timeout is specified.
  size_t Receive(char *data,
                 size_t size,
                 Time timeout = Time(0U));

  /// @return  May return 0 if timeout is specified.
  std::tuple<size_t, SocketAddress> ReceiveFrom(char *data,
                                                size_t size,
                                                Time timeout = Time(0U));

protected:
  Socket(int family, int type, int protocol);
  Socket(int fd);

protected:
  SocketGuard m_socketGuard;  ///< Guard to initialize socket subsystem on windows
  int m_fd;  ///< Socket file descriptor
};

class SocketUdp : public Socket
{
public:
  SocketUdp(SocketAddress const &bindAddress);

  void SendTo(char const *data,
              size_t size,
              SocketAddress const &dstAddress);
};

class SocketTcpClient : protected Socket
{
  friend class SocketTcpServer;

public:
  SocketTcpClient(SocketAddress const &connectAddress);

  void Send(char const *data,
            size_t size,
            Time timeout = Time(0U));

  using Socket::Receive;

private:
  SocketTcpClient(int fd);
};

class SocketTcpServer : protected Socket
{
public:
  SocketTcpServer(SocketAddress const &bindAddress);

  std::tuple<SocketTcpClient, SocketAddress> Listen(Time timeout = Time(0U));
};

#endif // SOCKET_H
