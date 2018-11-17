#ifndef SOCKET_ASYNC
#define SOCKET_ASYNC

#include "socket_buffered.h" // for SocketBuffered

#include <future> // for std::future
#include <functional> // for std::function
#include <memory> // for std::unique_ptr

class SocketDriver
{
  friend class SocketAsync;

public:
  SocketDriver();
  SocketDriver(SocketDriver const &) = delete;
  SocketDriver(SocketDriver &&other);
  ~SocketDriver();

  SocketDriver &operator=(SocketDriver const &) = delete;
  SocketDriver &operator=(SocketDriver &&other);

  void Step();

  void Run();
  void Stop();

  struct SocketDriverPriv;
private:
  std::shared_ptr<SocketDriverPriv> m_priv;
};

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
  SocketAsync(SocketAsync &&other);
  virtual ~SocketAsync();

  SocketAsync &operator=(SocketAsync const &other) = delete;
  SocketAsync &operator=(SocketAsync &&other);

protected:
  std::unique_ptr<SocketAsyncPriv> m_priv;
};

struct SocketUdpAsync : public SocketAsync
{
  SocketUdpAsync(SocketUdpBuffered &&buff,
                 SocketDriver &driver,
                 ReceiveHandler handleReceive);
  SocketUdpAsync(SocketUdpBuffered &&buff,
                 SocketDriver &driver,
                 ReceiveFromHandler handleReceiveFrom);

  SocketUdpAsync(SocketUdpAsync const &other) = delete;
  SocketUdpAsync(SocketUdpAsync &&other);

  SocketUdpAsync &operator=(SocketUdpAsync const &other) = delete;
  SocketUdpAsync &operator=(SocketUdpAsync &&other);

  /// Unreliably send data to address.
  /// @param  dstAddress  Address to send to; must match
  ///                     IP family of bound address.
  /// @throws  If sending fails locally.
  std::future<void> SendTo(SocketBufferPtr &&buffer,
                           SocketAddress const &dstAddress);
};

struct SocketTcpAsyncClient : public SocketAsync
{
  SocketTcpAsyncClient(SocketTcpBuffered &&buff,
                       SocketDriver &driver,
                       ReceiveHandler handleReceive,
                       DisconnectHandler handleDisconnect);

  SocketTcpAsyncClient(SocketTcpAsyncClient const &other) = delete;
  SocketTcpAsyncClient(SocketTcpAsyncClient &&other);

  SocketTcpAsyncClient &operator=(SocketTcpAsyncClient const &other) = delete;
  SocketTcpAsyncClient &operator=(SocketTcpAsyncClient &&other);

  /// Reliably send data to connected peer.
  /// @throws  If sending fails.
  std::future<void> Send(SocketBufferPtr &&buffer);
};

struct SocketTcpAsyncServer : public SocketAsync
{
  SocketTcpAsyncServer(SocketTcpServer &&sock,
                       SocketDriver &driver,
                       ConnectHandler handleConnect);
};

#endif // SOCKET_ASYNC
