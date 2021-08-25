#include "socket_async_impl.h"
#include "driver_impl.h" // for DriverImpl

#include <cassert> // for assert
#include <stdexcept> // for std::runtime_error
#include <type_traits> // for std::is_same_v

namespace sockpuppet {

// UDP socket with ReceiveFrom and SendTo
SocketAsyncImpl::SocketAsyncImpl(
    std::unique_ptr<SocketBufferedImpl> &&buff,
    DriverShared &driver,
    ReceiveFromHandler receiveFromHandler)
  : buff(std::move(buff))
  , driver(driver)
  , onReadable(std::bind(
      &SocketAsyncImpl::DriverDoFdTaskReceiveFrom,
      this,
      std::move(receiveFromHandler)))
  , onError([]() {}) // silently discard UDP receive errors
  , sendQ(std::in_place_type<SendToQ>)
{
  driver->AsyncRegister(*this);
}

// TCP socket with Receive and Send
SocketAsyncImpl::SocketAsyncImpl(
    std::unique_ptr<SocketBufferedImpl> &&buff,
    DriverShared &driver,
    ReceiveHandler receiveHandler,
    DisconnectHandler disconnectHandler)
  : buff(std::move(buff))
  , driver(driver)
  , onReadable(std::bind(
      &SocketAsyncImpl::DriverDoFdTaskReceive,
      this,
      std::move(receiveHandler)))
  , onError(std::bind(
      &SocketAsyncImpl::DriverDoFdTaskDisconnect,
      this,
      std::move(disconnectHandler),
      this->buff->sock->GetPeerName())) // cache remote address now before disconnect
  , sendQ(std::in_place_type<SendQ>)
{
  driver->AsyncRegister(*this);
}

// TCP acceptor with Listen/Accept
SocketAsyncImpl::SocketAsyncImpl(
    std::unique_ptr<SocketImpl> &&sock,
    DriverShared &driver,
    ConnectHandler connectHandler)
  : buff(std::make_unique<SocketBufferedImpl>(std::move(sock), 0U, 0U))
  , driver(driver)
  , onReadable(std::bind(
      &SocketAsyncImpl::DriverDoFdTaskConnect,
      this,
      std::move(connectHandler)))
  , onError([]() {}) // silently discard TCP accept errors
{
  driver->AsyncRegister(*this);
}

SocketAsyncImpl::~SocketAsyncImpl()
{
  if(auto const ptr = driver.lock()) {
    ptr->AsyncUnregister(buff->sock->fd);
  }
}

std::future<void> SocketAsyncImpl::Send(BufferPtr &&buffer)
{
  return DoSend<SendQ>(std::move(buffer));
}

std::future<void> SocketAsyncImpl::SendTo(BufferPtr &&buffer, AddressShared dstAddr)
{
  return DoSend<SendToQ>(std::move(buffer), std::move(dstAddr));
}

template<typename Queue, typename... Args>
std::future<void> SocketAsyncImpl::DoSend(Args&&... args)
{
  std::promise<void> promise;
  auto ret = promise.get_future();

  bool const wasEmpty = DoSendEnqueue<Queue>(
      std::move(promise), std::forward<Args>(args)...);
  if(wasEmpty) {
    if(auto const ptr = driver.lock()) {
      ptr->AsyncWantSend(buff->sock->fd);
    }
  }

  return ret;
}

template<typename Queue, typename... Args>
bool SocketAsyncImpl::DoSendEnqueue(std::promise<void> promise, Args&&... args)
{
  std::lock_guard<std::mutex> lock(sendQMtx);

  auto &q = std::get<Queue>(sendQ);
  bool const wasEmpty = q.empty();
  q.emplace(std::move(promise), std::forward<Args>(args)...);
  return wasEmpty;
}

template<typename Queue>
bool SocketAsyncImpl::IsSendQueueEmpty() const
{
  std::lock_guard<std::mutex> lock(sendQMtx);
  return std::get<Queue>(sendQ).empty();
}

void SocketAsyncImpl::DriverDoFdTaskReadable()
{
  onReadable();
}

void SocketAsyncImpl::DriverDoFdTaskConnect(ConnectHandler const &onConnect)
{
  try {
    auto [sock, addr] = buff->sock->Accept();
    buff->sock->Listen();

    onConnect(std::move(sock), std::move(addr));
  } catch(std::runtime_error const &) {
    onError();
  }
}

void SocketAsyncImpl::DriverDoFdTaskReceive(ReceiveHandler const &onReceive)
{
  try {
    auto buffer = buff->Receive();
    if(buffer->empty()) { // zero-size receipt
      // TLS socket received handshake data only: if we previously attempted
      // to send, there might still be data in the send queue now
      if(!IsSendQueueEmpty<SendQ>()) {
        if(auto const ptr = driver.lock()) {
          ptr->AsyncWantSend(buff->sock->fd);
        }
      }
    } else {
      onReceive(std::move(buffer));
    }
  } catch(std::runtime_error const &) {
    onError();
  }
}

void SocketAsyncImpl::DriverDoFdTaskReceiveFrom(ReceiveFromHandler const &onReceiveFrom)
{
  try {
    auto [buffer, addr] = buff->ReceiveFrom();
    onReceiveFrom(std::move(buffer), std::move(addr));
  } catch(std::runtime_error const &) {
    onError();
  }
}

bool SocketAsyncImpl::DriverDoFdTaskWritable()
{
  // hold the lock during send/sendto
  // as we already checked that the socket will not block and
  // otherwise we would need to re-lock afterwards to verify that
  // the previously empty queue has not been refilled asynchronously
  std::lock_guard<std::mutex> lock(sendQMtx);

  return std::visit([this](auto &&q) -> bool {
    using Q = std::decay_t<decltype(q)>;
    if constexpr(std::is_same_v<Q, SendQ>) {
      return DriverDoSend(q);
    } else if constexpr(std::is_same_v<Q, SendToQ>) {
      return DriverDoSendTo(q);
    }
  }, sendQ);
}

bool SocketAsyncImpl::DriverDoSend(SendQ &q)
{
  auto const sendQSize = q.size();
  if(!sendQSize) {
    throw std::logic_error("uncalled send");
  }

  auto &&[promise, buffer] = q.front();
  try {
    // allow partial send to avoid starving other
    // driver's sockets if this one is rate limited
    auto const sent = buff->sock->SendSome(buffer->data(), buffer->size());
    if(sent == buffer->size()) {
      promise.set_value();
    } else if(sent != 0U) {
      buffer->erase(0, sent);
      return false;
    } else { // zero-size sent data
      // TLS socket can't send data as it has not received handshake data from peer yet:
      // give up sending now but keep the data in the send queue to retry after receipt
      return true;
    }
  } catch(std::runtime_error const &e) {
    promise.set_exception(std::make_exception_ptr(e));
  }
  q.pop();
  return (sendQSize == 1U);
}

bool SocketAsyncImpl::DriverDoSendTo(SendToQ &q)
{
  auto const sendToQSize = q.size();
  if(!sendToQSize) {
    throw std::logic_error("uncalled sendto");
  }

  auto &&[promise, buffer, addr] = q.front();
  try {
    auto const sent = buff->sock->SendTo(buffer->data(), buffer->size(), addr->ForUdp());
    assert(sent == buffer->size());
    promise.set_value();
  } catch(std::runtime_error const &e) {
    promise.set_exception(std::make_exception_ptr(e));
  }
  q.pop();
  return (sendToQSize == 1U);
}

void SocketAsyncImpl::DriverDoFdTaskError()
{
  onError();
}

void SocketAsyncImpl::DriverDoFdTaskDisconnect(
    DisconnectHandler const &onDisconnect, AddressShared peerAddr)
{
  onDisconnect(Address(std::move(peerAddr)));
}

} // namespace sockpuppet
