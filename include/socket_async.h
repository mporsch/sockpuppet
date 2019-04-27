#ifndef SOCKET_ASYNC_H
#define SOCKET_ASYNC_H

#include "socket_address.h" // for SocketAddress
#include "socket_buffered.h" // for SocketBuffered

#include <future> // for std::future
#include <functional> // for std::function
#include <memory> // for std::unique_ptr
#include <tuple> // for std::tuple

/// Socket driver that runs one or multiple attached socket classes and
/// may be run by a dedicated thread or stepped iteratively.
class SocketDriver
{
  friend class SocketAsync;

public:
  /// Create a socket driver that can be passed to sockets to attach to.
  SocketDriver();

  SocketDriver(SocketDriver const &) = delete;
  SocketDriver(SocketDriver &&other) noexcept;
  ~SocketDriver();

  SocketDriver &operator=(SocketDriver const &) = delete;
  SocketDriver &operator=(SocketDriver &&other) noexcept;

  /// Run one iteration on the attached sockets.
  /// @param  timeout  Timeout to use; 0 allows blocking if
  ///                  all attached sockets are idle.
  void Step(SocketBuffered::Time timeout = SocketBuffered::Time(0));

  /// Continuously run the attached sockets.
  /// @note  Blocking call. Returns only after Stop() from another thread.
  void Run();

  /// Cancel the continuously running Run() method.
  void Stop();

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
  using Time = SocketBuffered::Time;
  using SocketBuffer = SocketBuffered::SocketBuffer;
  using SocketBufferPtr = SocketBuffered::SocketBufferPtr;
  using ReceiveHandler = std::function<
    void(SocketBuffered::SocketBufferPtr)
  >;
  using ReceiveFromHandler = std::function<
    void(std::tuple<SocketBuffered::SocketBufferPtr, SocketAddress>)
  >;
  using ConnectHandler = std::function<
    void(std::tuple<SocketTcpClient, SocketAddress>)
  >;
  using DisconnectHandler = std::function<
    void(SocketAddress)
  >;

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
  SocketUdpAsync(SocketUdpBuffered &&buff,
                 SocketDriver &driver,
                 ReceiveHandler handleReceive);

  /// Create a UDP socket driven by given socket driver.
  /// @param  buff  Buffered UDP socket to augment.
  /// @param  driver  Socket driver to run the socket.
  /// @param  handleReceiveFrom  (Bound) function to call on receipt.
  SocketUdpAsync(SocketUdpBuffered &&buff,
                 SocketDriver &driver,
                 ReceiveFromHandler handleReceiveFrom);

  SocketUdpAsync(SocketUdpAsync const &other) = delete;
  SocketUdpAsync(SocketUdpAsync &&other) noexcept;
  ~SocketUdpAsync() override;

  SocketUdpAsync &operator=(SocketUdpAsync const &other) = delete;
  SocketUdpAsync &operator=(SocketUdpAsync &&other) noexcept;

  /// Enqueue data to unreliably send to address.
  /// @param  dstAddress  Address to send to; must match
  ///                     IP family of bound address.
  /// @return  Future object to fulfill when data was actually sent.
  std::future<void> SendTo(SocketBufferPtr &&buffer,
                           SocketAddress const &dstAddress);
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
  /// @return  Future object to fulfill when data was actually sent.
  std::future<void> Send(SocketBufferPtr &&buffer);
};

/// TCP (reliable communication) socket class that adds an interface for
/// an external socket driver to the regular TCP server class.
struct SocketTcpAsyncServer : public SocketAsync
{
  /// Create a TCP server socket driven by given socket driver.
  /// @param  sock  TCP server socket to augment.
  /// @param  driver  Socket driver to run the socket.
  /// @param  handleConnect  (Bound) function to call when a TCP client connects.
  SocketTcpAsyncServer(SocketTcpServer &&sock,
                       SocketDriver &driver,
                       ConnectHandler handleConnect);
};

#endif // SOCKET_ASYNC_H
