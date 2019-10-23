#ifndef SOCKPUPPET_SOCKET_ASYNC_H
#define SOCKPUPPET_SOCKET_ASYNC_H

#include "sockpuppet/address.h" // for Address
#include "sockpuppet/socket.h" // for Socket
#include "sockpuppet/socket_buffered.h" // for SocketBuffered

#include <functional> // for std::function
#include <future> // for std::future
#include <memory> // for std::unique_ptr

namespace sockpuppet {

/// Socket driver that runs one or multiple attached socket classes and
/// may be driven by a dedicated thread or stepped iteratively.
class SocketDriver
{
  friend class SocketAsync;

public:
  using Duration = SocketBuffered::Duration;

  /// Create a socket driver that can be passed to sockets to attach to.
  /// @throws  If creating the internal event signalling fails.
  SocketDriver();

  SocketDriver(SocketDriver const &) = delete;
  SocketDriver(SocketDriver &&other) noexcept;
  ~SocketDriver();
  SocketDriver &operator=(SocketDriver const &) = delete;
  SocketDriver &operator=(SocketDriver &&other) noexcept;

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

  /// Pimpl to hide away the OS-specifics.
  struct SocketDriverPriv;

private:
  std::shared_ptr<SocketDriverPriv> m_priv;
};

/// The externally driven socket base class stores the user handlers and
/// the link to the socket driver.
/// It is created by its derived classes and is not intended to
/// be created by the user.
class SocketAsync
{
public:
  using SocketBufferPool = SocketBuffered::SocketBufferPool;
  using SocketBufferPtr = SocketBuffered::SocketBufferPtr;
  using ReceiveHandler = std::function<void(SocketBufferPtr)>;
  using ReceiveFromHandler = std::function<void(SocketBufferPtr, Address)>;
  using ConnectHandler = std::function<void(SocketTcpClient, Address)>;
  using DisconnectHandler = std::function<void(Address)>;

  /// Get the local (bound-to) address of the socket.
  /// @throws  If the address lookup fails.
  Address LocalAddress() const;

  /// Determine the maximum size of data the socket may receive,
  /// i.e. the size the OS has allocated for its receive buffer.
  /// This might be much more than the ~1500 bytes expected.
  /// @throws  If getting the socket parameter fails.
  size_t ReceiveBufferSize() const;

  /// Pimpl to hide away the OS-specifics.
  struct SocketAsyncPriv;

protected:
  struct Handlers
  {
    ReceiveHandler receive;
    ReceiveFromHandler receiveFrom;
    ConnectHandler connect;
    DisconnectHandler disconnect;
  };

protected:
  SocketAsync(Socket &&sock,
              SocketDriver &driver,
              Handlers handlers);
  SocketAsync(SocketBuffered &&buff,
              SocketDriver &driver,
              Handlers handlers);
  SocketAsync(SocketAsync const &other) = delete;
  SocketAsync(SocketAsync &&other) noexcept;
  virtual ~SocketAsync();
  SocketAsync &operator=(SocketAsync const &other) = delete;
  SocketAsync &operator=(SocketAsync &&other) noexcept;

protected:
  std::unique_ptr<SocketAsyncPriv> m_priv;
};

/// UDP (unreliable communication) socket class that adds an interface for
/// an external socket driver to the buffered UDP class.
struct SocketUdpAsync : public SocketAsync
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

  SocketUdpAsync(SocketUdpAsync const &other) = delete;
  SocketUdpAsync(SocketUdpAsync &&other) noexcept;
  ~SocketUdpAsync() override;
  SocketUdpAsync &operator=(SocketUdpAsync const &other) = delete;
  SocketUdpAsync &operator=(SocketUdpAsync &&other) noexcept;

  /// Enqueue data to unreliably send to address.
  /// @param  buffer  Borrowed buffer to enqueue for send and release after completition.
  ///                 Create using your own SocketBufferPool.
  /// @param  dstAddress  Address to send to; must match
  ///                     IP family of bound address.
  /// @return  Future object to fulfill when data was actually sent.
  std::future<void> SendTo(SocketBufferPtr &&buffer,
                           Address const &dstAddress);
};

/// TCP (reliable communication) socket class that adds an interface for
/// an external socket driver to the buffered TCP client class.
struct SocketTcpAsyncClient : public SocketAsync
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

  SocketTcpAsyncClient(SocketTcpAsyncClient const &other) = delete;
  SocketTcpAsyncClient(SocketTcpAsyncClient &&other) noexcept;
  ~SocketTcpAsyncClient() override;
  SocketTcpAsyncClient &operator=(SocketTcpAsyncClient const &other) = delete;
  SocketTcpAsyncClient &operator=(SocketTcpAsyncClient &&other) noexcept;

  /// Enqueue data to reliably send to connected peer.
  /// @param  buffer  Borrowed buffer to enqueue for send and release after completition.
  ///                 Create using your own SocketBufferPool.
  /// @return  Future object to fulfill when data was actually sent.
  std::future<void> Send(SocketBufferPtr &&buffer);

  /// Get the remote peer address of the socket.
  /// @throws  If the address lookup fails.
  Address PeerAddress() const;
};

/// TCP (reliable communication) socket class that adds an interface for
/// an external socket driver to the regular TCP server class.
struct SocketTcpAsyncServer : public SocketAsync
{
  /// Create a TCP server socket driven by given socket driver.
  /// @param  sock  TCP server socket to augment.
  /// @param  driver  Socket driver to run the socket.
  /// @param  handleConnect  (Bound) function to call when a TCP client connects.
  /// @throws  If an invalid handler is provided.
  SocketTcpAsyncServer(SocketTcpServer &&sock,
                       SocketDriver &driver,
                       ConnectHandler handleConnect);

  SocketTcpAsyncServer(SocketTcpAsyncServer const &other) = delete;
  SocketTcpAsyncServer(SocketTcpAsyncServer &&other) noexcept;
  ~SocketTcpAsyncServer() override;
  SocketTcpAsyncServer &operator=(SocketTcpAsyncServer const &other) = delete;
  SocketTcpAsyncServer &operator=(SocketTcpAsyncServer &&other) noexcept;
};

} // namespace sockpuppet

#endif // SOCKPUPPET_SOCKET_ASYNC_H
