#ifndef SOCKPUPPET_SOCKET_ASYNC_H
#define SOCKPUPPET_SOCKET_ASYNC_H

#include "sockpuppet/address.h" // for Address
#include "sockpuppet/socket.h" // for Duration
#include "sockpuppet/socket_buffered.h" // for BufferPtr

#include <functional> // for std::function
#include <future> // for std::future
#include <memory> // for std::unique_ptr

namespace sockpuppet {

/// Socket driver that runs one or multiple attached socket classes and
/// may be driven by a dedicated thread or stepped iteratively.
struct SocketDriver
{
  /// Create a socket driver that can be passed to sockets to attach to.
  /// @throws  If creating the internal event signalling fails.
  SocketDriver();

  uint64_t Schedule(Duration delay, std::function<void()> what);
  void Unschedule(uint64_t id);
  void Reschedule(uint64_t id, Duration delay);

  /// Run one iteration on the attached sockets.
  /// @param  timeout  Timeout to use; non-null allows blocking if
  ///                  all attached sockets are idle,
  ///                  a negative value allows unlimited blocking.
  /// @throws  If the internal event handling fails.
  void Step(Duration timeout = Duration(-1));

  /// Continuously run the attached sockets.
  /// @throws  If the internal event handling fails.
  /// @note  Blocking call. Returns only after Stop() from another thread.
  void Run();

  /// Cancel the continuously running Run() method.
  /// @throws  If the internal event signalling fails.
  void Stop();

  SocketDriver(SocketDriver const &) = delete;
  SocketDriver(SocketDriver &&other) noexcept;
  ~SocketDriver();
  SocketDriver &operator=(SocketDriver const &) = delete;
  SocketDriver &operator=(SocketDriver &&other) noexcept;

  /// Bridge to hide away the OS-specifics.
  struct SocketDriverPriv;
  std::shared_ptr<SocketDriverPriv> priv;
};

struct SocketAsyncPriv;
using ReceiveHandler = std::function<void(BufferPtr)>;
using ReceiveFromHandler = std::function<void(BufferPtr, Address)>;
using ConnectHandler = std::function<void(SocketTcpClient, Address)>;
using DisconnectHandler = std::function<void(Address)>;

/// UDP (unreliable communication) socket class that adds an interface for
/// an external socket driver to the buffered UDP class.
struct SocketUdpAsync
{
  /// Create a UDP socket driven by given socket driver.
  /// @param  buff  Buffered UDP socket to augment.
  /// @param  driver  Socket driver to run the socket.
  /// @param  handleReceive  (Bound) function to call on receipt.
  /// @throws  If an invalid handler is provided.
  SocketUdpAsync(SocketUdpBuffered &&buff,
                 SocketDriver &driver,
                 ReceiveHandler handleReceive);

  /// Create a UDP socket driven by given socket driver.
  /// @param  buff  Buffered UDP socket to augment.
  /// @param  driver  Socket driver to run the socket.
  /// @param  handleReceiveFrom  (Bound) function to call on receipt.
  /// @throws  If an invalid handler is provided.
  SocketUdpAsync(SocketUdpBuffered &&buff,
                 SocketDriver &driver,
                 ReceiveFromHandler handleReceiveFrom);

  /// Enqueue data to unreliably send to address.
  /// @param  buffer  Borrowed buffer to enqueue for send and release after completition.
  ///                 Create using your own BufferPool.
  /// @param  dstAddress  Address to send to; must match
  ///                     IP family of bound address.
  /// @return  Future object to fulfill when data was actually sent.
  std::future<void> SendTo(BufferPtr &&buffer,
                           Address const &dstAddress);

  /// Get the local (bound-to) address of the socket.
  /// @throws  If the address lookup fails.
  Address LocalAddress() const;

  SocketUdpAsync(SocketUdpAsync const &other) = delete;
  SocketUdpAsync(SocketUdpAsync &&other) noexcept;
  ~SocketUdpAsync();
  SocketUdpAsync &operator=(SocketUdpAsync const &other) = delete;
  SocketUdpAsync &operator=(SocketUdpAsync &&other) noexcept;

  /// Bridge to hide away the OS-specifics.
  std::unique_ptr<SocketAsyncPriv> priv;
};

/// TCP (reliable communication) socket class that adds an interface for
/// an external socket driver to the buffered TCP client class.
struct SocketTcpAsyncClient
{
  /// Create a TCP client socket driven by given socket driver.
  /// @param  buff  Buffered TCP client socket to augment.
  /// @param  driver  Socket driver to run the socket.
  /// @param  handleReceive  (Bound) function to call on receipt from
  ///                        connected peer.
  /// @param  handleDisconnect  (Bound) function to call when socket was
  ///                           disconnected and has become invalid.
  /// @throws  If an invalid handler is provided.
  SocketTcpAsyncClient(SocketTcpBuffered &&buff,
                       SocketDriver &driver,
                       ReceiveHandler handleReceive,
                       DisconnectHandler handleDisconnect);

  /// Enqueue data to reliably send to connected peer.
  /// @param  buffer  Borrowed buffer to enqueue for send and release after completition.
  ///                 Create using your own BufferPool.
  /// @return  Future object to fulfill when data was actually sent.
  std::future<void> Send(BufferPtr &&buffer);

  /// Get the local (bound-to) address of the socket.
  /// @throws  If the address lookup fails.
  Address LocalAddress() const;

  /// Get the remote peer address of the socket.
  /// @throws  If the address lookup fails.
  Address PeerAddress() const;

  SocketTcpAsyncClient(SocketTcpAsyncClient const &other) = delete;
  SocketTcpAsyncClient(SocketTcpAsyncClient &&other) noexcept;
  ~SocketTcpAsyncClient();
  SocketTcpAsyncClient &operator=(SocketTcpAsyncClient const &other) = delete;
  SocketTcpAsyncClient &operator=(SocketTcpAsyncClient &&other) noexcept;

  /// Bridge to hide away the OS-specifics.
  std::unique_ptr<SocketAsyncPriv> priv;
};

/// TCP (reliable communication) socket class that adds an interface for
/// an external socket driver to the regular TCP server class.
struct SocketTcpAsyncServer
{
  /// Create a TCP server socket driven by given socket driver.
  /// @param  sock  TCP server socket to augment.
  /// @param  driver  Socket driver to run the socket.
  /// @param  handleConnect  (Bound) function to call when a TCP client connects.
  /// @throws  If an invalid handler is provided.
  SocketTcpAsyncServer(SocketTcpServer &&sock,
                       SocketDriver &driver,
                       ConnectHandler handleConnect);

  /// Get the local (bound-to) address of the socket.
  /// @throws  If the address lookup fails.
  Address LocalAddress() const;

  SocketTcpAsyncServer(SocketTcpAsyncServer const &other) = delete;
  SocketTcpAsyncServer(SocketTcpAsyncServer &&other) noexcept;
  ~SocketTcpAsyncServer();
  SocketTcpAsyncServer &operator=(SocketTcpAsyncServer const &other) = delete;
  SocketTcpAsyncServer &operator=(SocketTcpAsyncServer &&other) noexcept;

  /// Bridge to hide away the OS-specifics.
  std::unique_ptr<SocketAsyncPriv> priv;
};

} // namespace sockpuppet

#endif // SOCKPUPPET_SOCKET_ASYNC_H
