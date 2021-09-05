#ifndef SOCKPUPPET_SOCKET_ASYNC_IMPL_H
#define SOCKPUPPET_SOCKET_ASYNC_IMPL_H

#include "address_impl.h" // for SockAddrStorage
#include "socket_buffered_impl.h" // for SocketBufferedImpl
#include "sockpuppet/address.h" // for Address
#include "sockpuppet/socket_async.h" // for Driver

#include <future> // for std::future
#include <memory> // for std::shared_ptr
#include <mutex> // for std::mutex
#include <queue> // for std::queue
#include <tuple> // for std::tuple
#include <variant> // for std::variant

namespace sockpuppet {

struct SocketAsyncImpl
{
  using AddressShared = std::shared_ptr<Address::AddressImpl>;
  using DriverShared = std::shared_ptr<Driver::DriverImpl>;
  using SendQElement = std::tuple<std::promise<void>, BufferPtr>;
  using SendQ = std::queue<SendQElement>;
  using SendToQElement = std::tuple<std::promise<void>, BufferPtr, AddressShared>;
  using SendToQ = std::queue<SendToQElement>;

  std::unique_ptr<SocketBufferedImpl> buff;
  std::weak_ptr<Driver::DriverImpl> driver;
  std::function<void()> onReadable; // contains use-case-dependent data as bound arguments
  std::function<void(char const *)> onError; // contains use-case-dependent data as bound arguments
  mutable std::mutex sendQMtx;
  std::variant<SendQ, SendToQ> sendQ; // use-case dependent queue type
  bool pendingTlsSend = false; // flag to satisfy OpenSSL_write retry requirements

  SocketAsyncImpl(std::unique_ptr<SocketBufferedImpl> &&buff,
                  DriverShared &driver,
                  ReceiveFromHandler onReceiveFrom);
  SocketAsyncImpl(std::unique_ptr<SocketBufferedImpl> &&buff,
                  DriverShared &driver,
                  ReceiveHandler onReceive,
                  DisconnectHandler onDisconnect);
  SocketAsyncImpl(std::unique_ptr<SocketImpl> &&sock,
                  DriverShared &driver,
                  ConnectHandler onConnect);
  SocketAsyncImpl(SocketAsyncImpl const &) = delete;
  SocketAsyncImpl(SocketAsyncImpl &&) = delete;
  ~SocketAsyncImpl();
  SocketAsyncImpl &operator=(SocketAsyncImpl const &) = delete;
  SocketAsyncImpl &operator=(SocketAsyncImpl &&) = delete;

  std::future<void> Send(BufferPtr &&buffer);
  std::future<void> SendTo(BufferPtr &&buffer, AddressShared dstAddr);

  template<typename Queue, typename... Args>
  std::future<void> DoSend(Args&&... args);
  template<typename Queue, typename... Args>
  bool DoSendEnqueue(std::promise<void> promise, Args&&... args);

  // in thread context of DriverImpl
  void DriverOnReadable();
  void DriverConnect(ConnectHandler const &onConnect);
  void DriverReceive(ReceiveHandler const &onReceive);
  void DriverReceiveFrom(ReceiveFromHandler const &onReceiveFrom);

  /// @return  true if there is no more data to send, false otherwise
  bool DriverOnWritable();
  bool DriverSend(SendQ &q);
  bool DriverSendTo(SendToQ &q);

  void DriverOnError(char const *message);
  void DriverDisconnect(DisconnectHandler const &onDisconnect,
                        AddressShared peerAddr,
                        char const *errorMessage);
};

} // namespace sockpuppet

#endif // SOCKPUPPET_SOCKET_ASYNC_IMPL_H
