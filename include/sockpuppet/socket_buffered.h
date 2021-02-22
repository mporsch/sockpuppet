#ifndef SOCKPUPPET_SOCKET_BUFFERED_H
#define SOCKPUPPET_SOCKET_BUFFERED_H

#include "sockpuppet/address.h" // for Address
#include "sockpuppet/socket.h" // for Duration

#include <cstddef> // for size_t
#include <deque> // for std::deque
#include <memory> // for std::unique_ptr
#include <mutex> // for std::mutex
#include <optional> // for std::optional
#include <stack> // for std::stack
#include <string> // for std::string
#include <utility> // for std::pair

namespace sockpuppet {

/// Send/Receive buffer resource storage.
/// Internally stores two buffer lists; busy and idle.
/// Idle buffers may be obtained by the user and
/// are moved to the busy list. Once released,
/// it is automatically moved back again.
struct BufferPool
{
  using Buffer = std::string;
  struct Recycler
  {
    BufferPool *pool;

    void operator()(Buffer *buf);
  };
  using BufferPtr = std::unique_ptr<Buffer, Recycler>;

  /// Create a pool with given maximum number of buffers.
  /// @param  maxSize  Maximum number of buffers to maintain (0 -> unlimited).
  BufferPool(size_t maxSize = 0U);

  /// Obtain an idle buffer.
  /// @return  Pointer to borrowed buffer still owned by
  ///          the pool; the user must not change the pointer.
  /// @throws  If more buffers are obtained than initially agreed upon.
  /// @note  Mind that all buffers must be released before destroying the pool.
  BufferPtr Get();

  BufferPool(BufferPool const &other) = delete;
  BufferPool(BufferPool &&other) = delete;
  ~BufferPool();
  BufferPool &operator=(BufferPool const &other) = delete;
  BufferPool &operator=(BufferPool &&other) = delete;

private:
  void Recycle(Buffer *buf);

private:
  using BufferStorage = std::unique_ptr<Buffer>;

  size_t m_maxSize;
  std::mutex m_mtx;
  std::stack<BufferStorage> m_idle;
  std::deque<BufferStorage> m_busy;
};

struct SocketBufferedPriv;
using BufferPtr = BufferPool::BufferPtr;

/// UDP (unreliable communication) socket class that adds an internal
/// receive buffer pool to the regular UDP socket class.
struct SocketUdpBuffered
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

  /// Unreliably receive data on bound address and report the source.
  /// @param  timeout  Timeout to use; non-null causes blocking receipt,
  ///                  a negative value allows unlimited blocking.
  /// @return  Received data buffer borrowed from socket and source address.
  ///          May return empty buffer and invalid address
  ///          only if non-negative \p timeout is specified.
  /// @throws  If receipt fails locally or number of receive buffers is exceeded.
  std::optional<std::pair<BufferPtr, Address>>
  ReceiveFrom(Duration timeout = Duration(-1));

  /// Get the local (bound-to) address of the socket.
  /// @throws  If the address lookup fails.
  Address LocalAddress() const;

  SocketUdpBuffered(SocketUdpBuffered const &other) = delete;
  SocketUdpBuffered(SocketUdpBuffered &&other) noexcept;
  ~SocketUdpBuffered();
  SocketUdpBuffered &operator=(SocketUdpBuffered const &other) = delete;
  SocketUdpBuffered &operator=(SocketUdpBuffered &&other) noexcept;

  /// Bridge to hide away the OS-specifics.
  std::unique_ptr<SocketBufferedPriv> priv;
};

/// TCP (reliable communication) socket class that adds an internal
/// receive buffer pool to the regular TCP client socket class.
struct SocketTcpBuffered
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
  /// @param  timeout  Timeout to use; non-null causes blocking receipt,
  ///                  a negative value allows unlimited blocking.
  /// @return  Received data buffer borrowed from socket.
  ///          May return empty buffer only if non-negative \p timeout is specified.
  /// @throws  If the number of receive buffers is exceeded, receipt fails or
  ///          the peer closes the connection.
  std::optional<BufferPtr> Receive(Duration timeout = Duration(-1));

  /// Get the local (bound-to) address of the socket.
  /// @throws  If the address lookup fails.
  Address LocalAddress() const;

  /// Get the remote peer address of the socket.
  /// @throws  If the address lookup fails.
  Address PeerAddress() const;

  SocketTcpBuffered(SocketTcpBuffered const &other) = delete;
  SocketTcpBuffered(SocketTcpBuffered &&other) noexcept;
  ~SocketTcpBuffered();
  SocketTcpBuffered &operator=(SocketTcpBuffered const &other) = delete;
  SocketTcpBuffered &operator=(SocketTcpBuffered &&other) noexcept;

  /// Bridge to hide away the OS-specifics.
  std::unique_ptr<SocketBufferedPriv> priv;
};

} // namespace sockpuppet

#endif // SOCKPUPPET_SOCKET_BUFFERED_H
