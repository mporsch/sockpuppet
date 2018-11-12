#ifndef SOCKET_H
#define SOCKET_H

#include "socket_address.h" // for SocketAddress

#include <chrono> // for std::chrono
#include <cstddef> // for size_t
#include <memory> // for std::unique_ptr
#include <tuple> // for std::tuple

/// The socket base class stores the hidden implementation.
/// It is created by its derived classes and is not intended
/// to be created by the user.
class Socket
{
  friend class SocketBuffered;

public:
  using Time = std::chrono::duration<uint32_t, std::micro>;

  /// Determine the maximum size of data the socket may receive,
  /// i.e. the size the OS has allocated for its receive buffer.
  /// This might be much more than the ~1500 bytes expected.
  size_t GetReceiveBufferSize();

  struct SocketPriv;

protected:
  Socket(std::unique_ptr<SocketPriv> &&other);
  Socket(Socket const &other) = delete;
  Socket(Socket &&other);
  virtual ~Socket();

  Socket &operator=(Socket const &other) = delete;
  Socket &operator=(Socket &&other);

protected:
  std::unique_ptr<SocketPriv> m_priv;
};

/// UDP (unreliable communication) socket class that is
/// bound to provided address.
struct SocketUdp : public Socket
{
  /// Create a UDP socket bound to given address.
  /// @throws  If binding fails.
  SocketUdp(SocketAddress const &bindAddress);

  SocketUdp(SocketUdp const &other) = delete;
  SocketUdp(SocketUdp &&other);

  SocketUdp &operator=(SocketUdp const &other) = delete;
  SocketUdp &operator=(SocketUdp &&other);

  /// Unreliably send data to address.
  /// @param  dstAddress  Address to send to; must match
  ///                     IP family of bound address.
  /// @throws  If sending fails locally.
  void SendTo(char const *data,
              size_t size,
              SocketAddress const &dstAddress);

  /// Unreliably receive data on bound address.
  /// @param  timeout  Timeout to use; 0 causes blocking receipt.
  /// @return  May return 0 only if timeout is specified.
  /// @throws  If receipt fails locally.
  size_t Receive(char *data,
                 size_t size,
                 Time timeout = Time(0U));

  /// Unreliably receive data on bound address and report the source.
  /// @param  timeout  Timeout to use; 0 causes blocking receipt.
  /// @return  May return 0 and invalid address only if timeout is specified.
  /// @throws  If receipt fails locally.
  std::tuple<size_t, SocketAddress> ReceiveFrom(char *data,
                                                size_t size,
                                                Time timeout = Time(0U));
};

/// TCP (reliable communication) socket class that is either
/// connected to provided peer address or to a peer accepted
/// by the TCP server socket.
struct SocketTcpClient : public Socket
{
  /// Create a TCP socket connected to given address.
  /// @throws  If connect fails.
  SocketTcpClient(SocketAddress const &connectAddress);

  /// Constructor for internal use.
  SocketTcpClient(std::unique_ptr<Socket::SocketPriv> &&other);

  SocketTcpClient(SocketTcpClient const &other) = delete;
  SocketTcpClient(SocketTcpClient &&other);

  SocketTcpClient &operator=(SocketTcpClient const &other) = delete;
  SocketTcpClient &operator=(SocketTcpClient &&other);

  /// Reliably send data to connected peer.
  /// @param  timeout  Timeout to use; 0 causes blocking send.
  /// @throws  If sending fails.
  void Send(char const *data,
            size_t size,
            Time timeout = Time(0U));

  /// Reliably receive data from connected peer.
  /// @param  timeout  Timeout to use; 0 causes blocking receipt.
  /// @return  May return 0 only if timeout is specified.
  /// @throws  If receipt fails.
  size_t Receive(char *data,
                 size_t size,
                 Time timeout = Time(0U));
};

/// TCP (reliable communication) socket class that is
/// bound to provided address and can create client sockets
/// for incoming peer connections.
struct SocketTcpServer : public Socket
{
  /// Create a TCP server socket bound to given address.
  /// @throws  If binding fails.
  SocketTcpServer(SocketAddress const &bindAddress);

  SocketTcpServer(SocketTcpServer const &other) = delete;
  SocketTcpServer(SocketTcpServer &&other);

  SocketTcpServer &operator=(SocketTcpServer const &other) = delete;
  SocketTcpServer &operator=(SocketTcpServer &&other);

  /// Listen and accept incoming TCP connections and report the source.
  /// @param  timeout  Timeout to use; 0 causes blocking listen.
  /// @throws  If listen or accept fails or timeout occurs.
  std::tuple<SocketTcpClient, SocketAddress> Listen(Time timeout = Time(0U));
};

#endif // SOCKET_H
