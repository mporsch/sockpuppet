#ifndef SOCKPUPPET_SOCKET_H
#define SOCKPUPPET_SOCKET_H

#include "sockpuppet/address.h" // for Address

#include <chrono> // for std::chrono
#include <cstddef> // for size_t
#include <memory> // for std::unique_ptr
#include <utility> // for std::pair

namespace sockpuppet {

struct SocketPriv;
using Duration = std::chrono::milliseconds;

/// UDP (unreliable communication) socket class that is
/// bound to provided address.
struct SocketUdp
{
  /// Create a UDP socket bound to given address.
  /// @param  bindAddress  Local interface address to bind to.
  ///                      Unspecified service or port number 0
  ///                      binds to an OS-assigned port.
  /// @throws  If binding or configuration fails.
  SocketUdp(Address const &bindAddress);

  /// Unreliably send data to address.
  /// @param  data  Pointer to data to send.
  /// @param  size  Size of data to send.
  /// @param  dstAddress  Address to send to; must match
  ///                     IP family of bound address.
  /// @param  timeout  Timeout to use; non-null causes blocking send,
  ///                  a negative value allows unlimited blocking.
  /// @return  Number of bytes sent. Always matches \p size on negative \p timeout.
  /// @throws  If sending fails locally.
  size_t SendTo(char const *data,
                size_t size,
                Address const &dstAddress,
                Duration timeout = Duration(-1));

  /// Unreliably receive data on bound address.
  /// @param  data  Pointer to receive buffer to fill.
  /// @param  size  Available receive buffer size.
  /// @param  timeout  Timeout to use; non-null causes blocking receipt,
  ///                  a negative value allows unlimited blocking.
  /// @return  Filled receive buffer size. May return 0
  ///          only if non-negative \p timeout is specified.
  /// @throws  If receipt fails locally.
  size_t Receive(char *data,
                 size_t size,
                 Duration timeout = Duration(-1));

  /// Unreliably receive data on bound address and report the source.
  /// @param  data  Pointer to receive buffer to fill.
  /// @param  size  Available receive buffer size.
  /// @param  timeout  Timeout to use; non-null causes blocking receipt,
  ///                  a negative value allows unlimited blocking.
  /// @return  Filled receive buffer size and source address.
  ///          May return 0 size and invalid address only if
  ///          non-negative \p timeout is specified.
  /// @throws  If receipt fails locally.
  std::pair<size_t, Address> ReceiveFrom(char *data,
                                         size_t size,
                                         Duration timeout = Duration(-1));
  /// Get the local (bound-to) address of the socket.
  /// @throws  If the address lookup fails.
  Address LocalAddress() const;

  /// Determine the maximum size of data the socket may receive,
  /// i.e. the size the OS has allocated for its receive buffer.
  /// This might be much more than the ~1500 bytes expected.
  /// @throws  If getting the socket parameter fails.
  size_t ReceiveBufferSize() const;

  SocketUdp(SocketUdp const &other) = delete;
  SocketUdp(SocketUdp &&other) noexcept;
  ~SocketUdp();
  SocketUdp &operator=(SocketUdp const &other) = delete;
  SocketUdp &operator=(SocketUdp &&other) noexcept;

  /// Bridge to hide away the OS-specifics.
  std::unique_ptr<SocketPriv> priv;
};

/// TCP (reliable communication) socket class that is either
/// connected to provided peer address or to a peer accepted
/// by the TCP server socket.
struct SocketTcpClient
{
  /// Create a TCP socket connected to given address.
  /// @param  connectAddress  Peer address to connect to.
  /// @throws  If connect fails.
  SocketTcpClient(Address const &connectAddress);

  /// Reliably send data to connected peer.
  /// @param  data  Pointer to data to send.
  /// @param  size  Size of data to send.
  /// @param  timeout  Timeout to use; non-null causes blocking send,
  ///                  a negative value allows unlimited blocking.
  /// @return  Number of bytes sent. Always matches \p size on negative \p timeout.
  /// @throws  If sending fails locally or the peer closes the connection.
  size_t Send(char const *data,
              size_t size,
              Duration timeout = Duration(-1));

  /// Reliably receive data from connected peer.
  /// @param  data  Pointer to receive buffer to fill.
  /// @param  size  Available receive buffer size.
  /// @param  timeout  Timeout to use; non-null causes blocking receipt,
  ///                  a negative value allows unlimited blocking.
  /// @return  Filled receive buffer size. May return 0
  ///          only if non-negative \p timeout is specified.
  /// @throws  If receipt fails or the peer closes the connection.
  size_t Receive(char *data,
                 size_t size,
                 Duration timeout = Duration(-1));
  /// Get the local (bound-to) address of the socket.
  /// @throws  If the address lookup fails.
  Address LocalAddress() const;

  /// Get the remote peer address of the socket.
  /// @throws  If the address lookup fails.
  Address PeerAddress() const;

  /// Determine the maximum size of data the socket may receive,
  /// i.e. the size the OS has allocated for its receive buffer.
  /// This might be much more than the ~1500 bytes expected.
  /// @throws  If getting the socket parameter fails.
  size_t ReceiveBufferSize() const;

  SocketTcpClient(std::unique_ptr<SocketPriv> &&other);
  SocketTcpClient(SocketTcpClient const &other) = delete;
  SocketTcpClient(SocketTcpClient &&other) noexcept;
  ~SocketTcpClient();
  SocketTcpClient &operator=(SocketTcpClient const &other) = delete;
  SocketTcpClient &operator=(SocketTcpClient &&other) noexcept;

  /// Bridge to hide away the OS-specifics.
  std::unique_ptr<SocketPriv> priv;
};

/// TCP (reliable communication) socket class that is
/// bound to provided address and can create client sockets
/// for incoming peer connections.
struct SocketTcpServer
{
  /// Create a TCP server socket bound to given address.
  /// @param  bindAddress  Local interface address to bind to.
  ///                      Unspecified service or port number 0
  ///                      binds to an OS-assigned port.
  /// @throws  If binding or configuration fails.
  SocketTcpServer(Address const &bindAddress);

  /// Listen and accept incoming TCP connections and report the source.
  /// @param  timeout  Timeout to use; non-null causes blocking listen,
  ///                  a negative value allows unlimited blocking.
  /// @return  Connected client and its address.
  /// @throws  If listen or accept fails or timeout occurs.
  std::pair<SocketTcpClient, Address> Listen(Duration timeout = Duration(-1));

  /// Get the local (bound-to) address of the socket.
  /// @throws  If the address lookup fails.
  Address LocalAddress() const;

  SocketTcpServer(SocketTcpServer const &other) = delete;
  SocketTcpServer(SocketTcpServer &&other) noexcept;
  ~SocketTcpServer();
  SocketTcpServer &operator=(SocketTcpServer const &other) = delete;
  SocketTcpServer &operator=(SocketTcpServer &&other) noexcept;

  /// Bridge to hide away the OS-specifics.
  std::unique_ptr<SocketPriv> priv;
};

} // namespace sockpuppet

#endif // SOCKPUPPET_SOCKET_H
