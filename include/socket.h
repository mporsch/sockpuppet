#ifndef SOCKET_H
#define SOCKET_H

#include "socket_address.h" // for SocketAddress

#include <chrono> // for std::chrono
#include <cstddef> // for size_t
#include <memory> // for std::unique_ptr
#include <tuple> // for std::tuple

struct Socket
{
  using Time = std::chrono::duration<uint32_t, std::micro>;

  Socket(Socket const &other) = delete;
  Socket(Socket &&other);
  virtual ~Socket();

  Socket &operator=(Socket const &other) = delete;
  Socket &operator=(Socket &&other);

  struct SocketPriv;
protected:
  Socket(std::unique_ptr<SocketPriv> &&other);

protected:
  std::unique_ptr<SocketPriv> m_priv;
};

struct SocketUdp : public Socket
{
  SocketUdp(SocketAddress const &bindAddress);

  void SendTo(char const *data,
              size_t size,
              SocketAddress const &dstAddress);

  /// @return  May return 0 if timeout is specified.
  size_t Receive(char *data,
                 size_t size,
                 Time timeout = Time(0U));

  /// @return  May return 0 if timeout is specified.
  std::tuple<size_t, SocketAddress> ReceiveFrom(char *data,
                                                size_t size,
                                                Time timeout = Time(0U));
};

class SocketTcpClient : public Socket
{
  friend class SocketTcpServer;

public:
  SocketTcpClient(SocketAddress const &connectAddress);

  void Send(char const *data,
            size_t size,
            Time timeout = Time(0U));

  /// @return  May return 0 if timeout is specified.
  size_t Receive(char *data,
                 size_t size,
                 Time timeout = Time(0U));

private:
  SocketTcpClient(std::unique_ptr<Socket::SocketPriv> &&other);
};

struct SocketTcpServer : public Socket
{
  SocketTcpServer(SocketAddress const &bindAddress);

  std::tuple<SocketTcpClient, SocketAddress> Listen(Time timeout = Time(0U));
};

#endif // SOCKET_H
