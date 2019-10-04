#ifndef SOCKET_BUFFERED_H
#define SOCKET_BUFFERED_H

#include "resource_pool.h" // for ResourcePool
#include "socket_address.h" // for SocketAddress
#include "socket.h" // for Socket

#include <cstddef> // for size_t
#include <memory> // for std::unique_ptr
#include <tuple> // for std::tuple
#include <vector> // for std::vector

namespace sockpuppet {

/// The buffered socket base class stores the receive buffer pool.
/// It is created by its derived classes and is not intended to
/// be created by the user.
class SocketBuffered
{
  friend class SocketAsync;

public:
  using Duration = Socket::Duration;
  using SocketBuffer = std::vector<char>;
  using SocketBufferPool = ResourcePool<SocketBuffer>;
  using SocketBufferPtr = SocketBufferPool::ResourcePtr;

  /// Get the local (bound-to) address of the socket.
  /// @throws  If the address lookup fails.
  SocketAddress LocalAddress() const;

  /// Determine the maximum size of data the socket may receive,
  /// i.e. the size the OS has allocated for its receive buffer.
  /// This might be much more than the ~1500 bytes expected.
  /// @throws  If getting the socket parameter fails.
  size_t ReceiveBufferSize() const;

  struct SocketBufferedPriv;

protected:
  SocketBuffered(Socket &&sock,
                 size_t rxBufCount,
                 size_t rxBufSize);
  SocketBuffered(SocketBuffered const &other) = delete;
  SocketBuffered(SocketBuffered &&other) noexcept;
  virtual ~SocketBuffered();

  SocketBuffered &operator=(SocketBuffered const &other) = delete;
  SocketBuffered &operator=(SocketBuffered &&other) noexcept;

protected:
  std::unique_ptr<SocketBufferedPriv> m_priv;
};

/// UDP (unreliable communication) socket class that adds an internal
/// receive buffer pool to the regular UDP socket class.
struct SocketUdpBuffered : public SocketBuffered
{
  /// Create a UDP socket with additional internal buffer pool.
  /// @param  sock  UDP socket to augment.
  /// @param  rxBufCount  Number of receive buffers to maintain (0 -> unlimited).
  ///                     Do not keep hold of more than this number of receive buffers!
  /// @param  rxBufSize  Size of each receive buffer.
  ///                    (0 -> use OS-determined maximum receive size.
  ///                     Careful! This might be outrageously more than
  ///                     what is actually needed.)
  /// @throws  If determining the receive buffer size fails.
  SocketUdpBuffered(SocketUdp &&sock,
                    size_t rxBufCount = 0U,
                    size_t rxBufSize = 0U);

  SocketUdpBuffered(SocketUdpBuffered const &other) = delete;
  SocketUdpBuffered(SocketUdpBuffered &&other) noexcept;
  ~SocketUdpBuffered() override;

  SocketUdpBuffered &operator=(SocketUdpBuffered const &other) = delete;
  SocketUdpBuffered &operator=(SocketUdpBuffered &&other) noexcept;

  /// Unreliably send data to address.
  /// @param  dstAddress  Address to send to; must match
  ///                     IP family of bound address.
  /// @throws  If sending fails locally.
  void SendTo(char const *data,
              size_t size,
              SocketAddress const &dstAddress);

  /// Unreliably receive data on bound address.
  /// @param  timeout  Timeout to use; non-null causes blocking receipt,
  ///                  a negative value allows unlimited blocking.
  /// @return  Received data buffer borrowed from socket.
  ///          May return empty buffer only if non-negative timeout is specified.
  /// @throws  If receipt fails locally or number of receive buffers is exceeded.
  SocketBufferPtr Receive(Duration timeout = Duration(-1));

  /// Unreliably receive data on bound address and report the source.
  /// @param  timeout  Timeout to use; non-null causes blocking receipt,
  ///                  a negative value allows unlimited blocking.
  /// @return  Received data buffer borrowed from socket and source address.
  ///          May return empty buffer and invalid address only if non-negative timeout is specified.
  /// @throws  If receipt fails locally or number of receive buffers is exceeded.
  std::tuple<SocketBufferPtr, SocketAddress> ReceiveFrom(Duration timeout = Duration(-1));
};

/// TCP (reliable communication) socket class that adds an internal
/// receive buffer pool to the regular TCP client socket class.
struct SocketTcpBuffered : public SocketBuffered
{
  /// Create a TCP socket with additional internal buffer pool.
  /// @param  sock  TCP client socket to augment.
  /// @param  rxBufCount  Number of receive buffers to maintain (0 -> unlimited).
  ///                     Do not keep hold of more than this number of receive buffers!
  /// @param  rxBufSize  Size of each receive buffer.
  ///                    (0 -> use OS-determined maximum receive size.
  ///                     Careful! This might be outrageously more than
  ///                     what is actually needed.)
  /// @throws  If determining the receive buffer size fails.
  SocketTcpBuffered(SocketTcpClient &&sock,
                    size_t rxBufCount = 0U,
                    size_t rxBufSize = 0U);

  SocketTcpBuffered(SocketTcpBuffered const &other) = delete;
  SocketTcpBuffered(SocketTcpBuffered &&other) noexcept;
  ~SocketTcpBuffered() override;

  SocketTcpBuffered &operator=(SocketTcpBuffered const &other) = delete;
  SocketTcpBuffered &operator=(SocketTcpBuffered &&other) noexcept;

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
  /// @return  Received data buffer borrowed from socket.
  ///          May return empty buffer only if non-negative timeout is specified.
  /// @throws  If receipt fails or number of receive buffers is exceeded.
  SocketBufferPtr Receive(Duration timeout = Duration(-1));

  /// Get the remote peer address of the socket.
  /// @throws  If the address lookup fails.
  SocketAddress PeerAddress() const;
};

} // namespace sockpuppet

#endif // SOCKET_BUFFERED_H
