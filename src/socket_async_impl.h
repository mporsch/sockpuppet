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

  struct Handlers
  {
    ReceiveHandler receive;
    ReceiveFromHandler receiveFrom;
    ConnectHandler connect;
    DisconnectHandler disconnect;
  };

  std::unique_ptr<SocketBufferedImpl> buff;
  std::weak_ptr<Driver::DriverImpl> driver;
  std::mutex sendQMtx;
  std::variant<SendQ, SendToQ> sendQ;
  Handlers handlers;
  std::shared_ptr<SockAddrStorage> peerAddr;

  SocketAsyncImpl(std::unique_ptr<SocketImpl> &&sock, DriverShared &driver, Handlers handlers);
  SocketAsyncImpl(std::unique_ptr<SocketBufferedImpl> &&buff, DriverShared &driver, Handlers handlers);
  SocketAsyncImpl(SocketAsyncImpl const &) = delete;
  SocketAsyncImpl(SocketAsyncImpl &&) = delete;
  ~SocketAsyncImpl();
  SocketAsyncImpl &operator=(SocketAsyncImpl const &) = delete;
  SocketAsyncImpl &operator=(SocketAsyncImpl &&) = delete;

  std::future<void> Send(BufferPtr &&buffer);
  std::future<void> SendTo(BufferPtr &&buffer, AddressShared dstAddr);

  template<typename Queue, typename... Args>
  std::future<void> DoSend(Queue &q, Args&&... args);

  // in thread context of DriverImpl
  void DriverDoFdTaskReadable();

  /// @return  true if there is no more data to send, false otherwise
  bool DriverDoFdTaskWritable();
  bool DriverDoSend(SendQElement &t);
  void DriverDoSendTo(SendToQElement &t);

  void DriverDoFdTaskError();
};

} // namespace sockpuppet

#endif // SOCKPUPPET_SOCKET_ASYNC_IMPL_H
