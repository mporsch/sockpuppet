#ifndef SOCKET_BUFFERED_H
#define SOCKET_BUFFERED_H

#include "resource_pool.h" // for ResourcePool
#include "socket_address.h" // for SocketAddress
#include "socket.h" // for SocketUdp

#include <memory> // for std::unique_ptr
#include <vector> // for std::vector

/// The buffered socket base class stores the receive buffer pool.
/// It is created by its derived classes and is not intended to
/// be created by the user.
class SocketBuffered
{
public:
  using SocketBuffer = std::vector<char>;
  using SocketBufferPtr = ResourcePool<SocketBuffer>::ResourcePtr;

protected:
  SocketBuffered(size_t rxBufCount,
                 size_t rxBufSize);
  SocketBuffered(SocketBuffered const &other) = delete;
  SocketBuffered(SocketBuffered &&other);
  virtual ~SocketBuffered();

  SocketBuffered &operator=(SocketBuffered const &other) = delete;
  SocketBuffered &operator=(SocketBuffered &&other);

  SocketBufferPtr GetBuffer();

protected:
  std::unique_ptr<ResourcePool<SocketBuffer>> m_pool;
  size_t m_rxBufSize;
};

/// UDP (unreliable communication) socket class that has an internal
/// receive buffer pool and is bound to provided address.
class SocketUdpBuffered
  : public SocketUdp
  , public SocketBuffered
{
public:
  /// Create a UDP socket with internal buffer pool bound to given address.
  /// @param  rxBufCount  Number of receive buffers to maintain (0 -> unlimited).
  /// @param  rxBufSize  Size of each receive buffer.
  ///                    (0 -> use OS-determined maximum receive size.
  ///                     Careful! This might be outrageously more than
  ///                     what is actually needed.)
  /// @throws  If determining the receive buffer size fails.
  SocketUdpBuffered(SocketUdp &&sock,
                    size_t rxBufCount = 0U,
                    size_t rxBufSize = 0U);

  /// Unreliably receive data on bound address.
  /// @param  timeout  Timeout to use; 0 causes blocking receipt.
  /// @return  May return empty buffer only if timeout is specified.
  /// @throws  If receipt fails locally or number of receive buffers is exceeded.
  SocketBufferPtr Receive(Time timeout = Time(0));

  /// Unreliably receive data on bound address and report the source.
  /// @param  timeout  Timeout to use; 0 causes blocking receipt.
  /// @return  May return empty buffer and invalid address only if timeout is specified.
  /// @throws  If receipt fails locally or number of receive buffers is exceeded.
  std::tuple<SocketBufferPtr, SocketAddress> ReceiveFrom(Time timeout = Time(0));
};

/// TCP (reliable communication) socket class that has an internal
/// receive buffer pool and is either connected to provided peer address or
/// to a peer accepted by the TCP server socket.
class SocketTcpBuffered
  : public SocketTcpClient
  , public SocketBuffered
{
public:
  /// Create a TCP socket with internal buffer pool connected to given address.
  /// @param  rxBufCount  Number of receive buffers to maintain (0 -> unlimited).
  /// @param  rxBufSize  Size of each receive buffer.
  ///                    (0 -> use OS-determined maximum receive size.
  ///                     Careful! This might be outrageously more than
  ///                     what is actually needed.)
  /// @throws  If determining the receive buffer size fails.
  SocketTcpBuffered(SocketTcpClient &&sock,
                    size_t rxBufCount = 0U,
                    size_t rxBufSize = 0U);

  /// Reliably receive data from connected peer.
  /// @param  timeout  Timeout to use; 0 causes blocking receipt.
  /// @return  May return empty buffer only if timeout is specified.
  /// @throws  If receipt fails or number of receive buffers is exceeded.
  SocketBufferPtr Receive(Time timeout = Time(0U));
};

#endif // SOCKET_BUFFERED_H
