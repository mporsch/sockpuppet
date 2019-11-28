#ifndef SOCKPUPPET_SOCKET_BUFFERED_H
#define SOCKPUPPET_SOCKET_BUFFERED_H

#include "sockpuppet/address.h" // for Address
#include "sockpuppet/socket.h" // for Socket

#include <cstddef> // for size_t
#include <deque> // for std::deque
#include <functional> // for std::reference_wrapper
#include <memory> // for std::unique_ptr
#include <mutex> // for std::mutex
#include <stack> // for std::stack
#include <utility> // for std::pair
#include <vector> // for std::vector

namespace sockpuppet {

/// Send/Receive buffer resource storage.
/// Internally stores two buffer lists; busy and idle.
/// Idle buffers may be obtained by the user and
/// are moved to the busy list. Once released,
/// it is automatically moved back again.
struct BufferPool
{
  using Buffer = std::vector<char>;
  struct Recycler
  {
    std::reference_wrapper<BufferPool> pool;

    void operator()(Buffer *buf);
  };
  using BufferPtr = std::unique_ptr<Buffer, Recycler>;

  /// Create a pool with given maximum number of buffers.
  /// @param  maxSize  Maximum number of buffers to maintain (0 -> unlimited).
  BufferPool(size_t maxSize = 0U);

  BufferPool(BufferPool const &other) = delete;
  BufferPool(BufferPool &&other) = delete;
  ~BufferPool();
  BufferPool &operator=(BufferPool const &other) = delete;
  BufferPool &operator=(BufferPool &&other) = delete;

  /// Obtain an idle buffer.
  /// @return  Pointer to borrowed buffer still owned by
  ///          the pool; the user must not change the pointer.
  /// @throws  If more buffers are obtained than initially agreed upon.
  /// @note  Mind that all buffers must be released before destroying the pool.
  BufferPtr Get();

private:
  void Recycle(Buffer *buf);

private:
  using BufferStorage = std::unique_ptr<Buffer>;

  size_t m_maxSize;
  std::mutex m_mtx;
  std::stack<BufferStorage> m_idle;
  std::deque<BufferStorage> m_busy;
};

using BufferPtr = BufferPool::BufferPtr;

/// The buffered socket base class stores the receive buffer pool.
/// It is created by its derived classes and is not intended to
/// be created by the user.
class SocketBuffered
{
  friend class SocketAsync;

public:
  using Duration = Socket::Duration;

  /// Get the local (bound-to) address of the socket.
  /// @throws  If the address lookup fails.
  Address LocalAddress() const;

  /// Determine the maximum size of data the socket may receive,
  /// i.e. the size the OS has allocated for its receive buffer.
  /// This might be much more than the ~1500 bytes expected.
  /// @throws  If getting the socket parameter fails.
  size_t ReceiveBufferSize() const;

  /// Pimpl to hide away the OS-specifics.
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
  /// @param  timeout  Timeout to use; non-null causes blocking receipt,
  ///                  a negative value allows unlimited blocking.
  /// @return  Received data buffer borrowed from socket.
  ///          May return empty buffer only if non-negative \p timeout is specified.
  /// @throws  If receipt fails locally or number of receive buffers is exceeded.
  BufferPtr Receive(Duration timeout = Duration(-1));

  /// Unreliably receive data on bound address and report the source.
  /// @param  timeout  Timeout to use; non-null causes blocking receipt,
  ///                  a negative value allows unlimited blocking.
  /// @return  Received data buffer borrowed from socket and source address.
  ///          May return empty buffer and invalid address
  ///          only if non-negative \p timeout is specified.
  /// @throws  If receipt fails locally or number of receive buffers is exceeded.
  std::pair<BufferPtr, Address> ReceiveFrom(Duration timeout = Duration(-1));
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
  BufferPtr Receive(Duration timeout = Duration(-1));

  /// Get the remote peer address of the socket.
  /// @throws  If the address lookup fails.
  Address PeerAddress() const;
};

} // namespace sockpuppet

#endif // SOCKPUPPET_SOCKET_BUFFERED_H
