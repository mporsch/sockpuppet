#ifndef SOCKPUPPET_SOCKET_H
#define SOCKPUPPET_SOCKET_H

#include "sockpuppet/address.h" // for Address

#include <chrono> // for std::chrono
#include <cstddef> // for size_t
#include <memory> // for std::unique_ptr
#include <tuple> // for std::tuple

namespace sockpuppet {

/// The socket base class stores the hidden implementation.
/// It is created by its derived classes and is not intended
/// to be created by the user.
class Socket
{
  friend class SocketBuffered;
  friend class SocketAsync;

public:
  using Duration = std::chrono::milliseconds;

  /// Get the local (bound-to) address of the socket.
  /// @throws  If the address lookup fails.
  Address LocalAddress() const;

  /// Determine the maximum size of data the socket may receive,
  /// i.e. the size the OS has allocated for its receive buffer.
  /// This might be much more than the ~1500 bytes expected.
  /// @throws  If getting the socket parameter fails.
  size_t ReceiveBufferSize() const;

  /// Pimpl to hide away the OS-specifics.
  struct SocketPriv;

protected:
  Socket(std::unique_ptr<SocketPriv> &&other);
  Socket(Socket const &other) = delete;
  Socket(Socket &&other) noexcept;
  virtual ~Socket();
  Socket &operator=(Socket const &other) = delete;
  Socket &operator=(Socket &&other) noexcept;

protected:
  std::unique_ptr<SocketPriv> m_priv;
};

/// UDP (unreliable communication) socket class that is
/// bound to provided address.
struct SocketUdp : public Socket
{
  /// Create a UDP socket bound to given address.
  /// @throws  If binding fails.
  SocketUdp(Address const &bindAddress);

  SocketUdp(SocketUdp const &other) = delete;
  SocketUdp(SocketUdp &&other) noexcept;
  ~SocketUdp() override;
  SocketUdp &operator=(SocketUdp const &other) = delete;
  SocketUdp &operator=(SocketUdp &&other) noexcept;

  /// Unreliably send data to address.
  /// @param  dstAddress  Address to send to; must match
  ///                     IP family of bound address.
  /// @throws  If sending fails locally.
  void SendTo(char const *data,
              size_t size,
              Address const &dstAddress);

  /// Unreliably receive data on bound address.
  /// @param  timeout  Timeout to use; non-null causes blocking receipt,
  ///                  a negative value allows unlimited blocking.
  /// @return  Receipt size. May return 0 only if non-negative timeout is specified.
  /// @throws  If receipt fails locally.
  size_t Receive(char *data,
                 size_t size,
                 Duration timeout = Duration(-1));

  /// Unreliably receive data on bound address and report the source.
  /// @param  timeout  Timeout to use; non-null causes blocking receipt,
  ///                  a negative value allows unlimited blocking.
  /// @return  Receipt size and source address.
  ///          May return 0 and invalid address only if non-negative timeout is specified.
  /// @throws  If receipt fails locally.
  std::tuple<size_t, Address> ReceiveFrom(char *data,
                                                size_t size,
                                                Duration timeout = Duration(-1));
};

/// TCP (reliable communication) socket class that is either
/// connected to provided peer address or to a peer accepted
/// by the TCP server socket.
struct SocketTcpClient : public Socket
{
  /// Create a TCP socket connected to given address.
  /// @throws  If connect fails.
  SocketTcpClient(Address const &connectAddress);

  /// Constructor for internal use.
  SocketTcpClient(std::unique_ptr<Socket::SocketPriv> &&other);

  SocketTcpClient(SocketTcpClient const &other) = delete;
  SocketTcpClient(SocketTcpClient &&other) noexcept;
  ~SocketTcpClient() override;
  SocketTcpClient &operator=(SocketTcpClient const &other) = delete;
  SocketTcpClient &operator=(SocketTcpClient &&other) noexcept;

  /// Reliably send data to connected peer.
  /// @param  timeout  Timeout to use; non-null causes blocking send,
  ///                  a negative value allows unlimited blocking.
  /// @throws  If sending fails or times out.
  void Send(char const *data,
            size_t size,
            Duration timeout = Duration(-1));

  /// Reliably receive data from connected peer.
  /// @param  timeout  Timeout to use; non-null causes blocking receipt,
  ///                  a negative value allows unlimited blocking.
  /// @return  Receipt size. May return 0 only if non-negative timeout is specified.
  /// @throws  If receipt fails.
  size_t Receive(char *data,
                 size_t size,
                 Duration timeout = Duration(-1));

  /// Get the remote peer address of the socket.
  /// @throws  If the address lookup fails.
  Address PeerAddress() const;
};

/// TCP (reliable communication) socket class that is
/// bound to provided address and can create client sockets
/// for incoming peer connections.
struct SocketTcpServer : public Socket
{
  /// Create a TCP server socket bound to given address.
  /// @throws  If binding fails.
  SocketTcpServer(Address const &bindAddress);

  SocketTcpServer(SocketTcpServer const &other) = delete;
  SocketTcpServer(SocketTcpServer &&other) noexcept;
  ~SocketTcpServer() override;
  SocketTcpServer &operator=(SocketTcpServer const &other) = delete;
  SocketTcpServer &operator=(SocketTcpServer &&other) noexcept;

  /// Listen and accept incoming TCP connections and report the source.
  /// @param  timeout  Timeout to use; non-null causes blocking listen,
  ///                  a negative value allows unlimited blocking.
  /// @return  Connected client and its address.
  /// @throws  If listen or accept fails or timeout occurs.
  std::tuple<SocketTcpClient, Address> Listen(Duration timeout = Duration(-1));
};

} // namespace sockpuppet

#endif // SOCKPUPPET_SOCKET_H
