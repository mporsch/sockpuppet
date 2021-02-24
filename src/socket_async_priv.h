#ifndef SOCKPUPPET_SOCKET_ASYNC_PRIV_H
#define SOCKPUPPET_SOCKET_ASYNC_PRIV_H

#include "address_priv.h" // for SockAddrStorage
#include "socket_buffered_priv.h" // for SocketBufferedPriv
#include "sockpuppet/address.h" // for Address
#include "sockpuppet/socket_async.h" // for Driver

#include <future> // for std::future
#include <memory> // for std::shared_ptr
#include <mutex> // for std::mutex
#include <queue> // for std::queue
#include <tuple> // for std::tuple

namespace sockpuppet {

struct SocketAsyncPriv : public SocketBufferedPriv
{
  using AddressShared = std::shared_ptr<Address::AddressPriv>;
  using DriverShared = std::shared_ptr<Driver::DriverPriv>;
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

  std::weak_ptr<Driver::DriverPriv> driver;
  Handlers handlers;
  std::mutex sendQMtx;
  SendQ sendQ;
  SendToQ sendToQ;
  std::shared_ptr<SockAddrStorage> peerAddr;

  SocketAsyncPriv(SocketPriv &&sock, DriverShared &driver, Handlers handlers);
  SocketAsyncPriv(SocketBufferedPriv &&buff, DriverShared &driver, Handlers handlers);
  SocketAsyncPriv(SocketAsyncPriv const &) = delete;
  SocketAsyncPriv(SocketAsyncPriv &&) = delete;
  ~SocketAsyncPriv();
  SocketAsyncPriv &operator=(SocketAsyncPriv const &) = delete;
  SocketAsyncPriv &operator=(SocketAsyncPriv &&) = delete;

  std::future<void> Send(BufferPtr &&buffer);
  std::future<void> SendTo(BufferPtr &&buffer, AddressShared dstAddr);

  template<typename Queue, typename... Args>
  std::future<void> DoSend(Queue &q, Args&&... args);

  // in thread context of DriverPriv
  void DriverDoFdTaskReadable();

  /// @return  true if there is no more data to send, false otherwise
  bool DriverDoFdTaskWritable();
  bool DriverDoSend(SendQElement &t);
  void DriverDoSendTo(SendToQElement &t);

  void DriverDoFdTaskError();
};

} // namespace sockpuppet

#endif // SOCKPUPPET_SOCKET_ASYNC_PRIV_H
