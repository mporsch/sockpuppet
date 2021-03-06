#include "socket_async_priv.h"
#include "driver_priv.h" // for DriverPriv

#include <cassert> // for assert
#include <stdexcept> // for std::runtime_error

namespace sockpuppet {

SocketAsyncPriv::SocketAsyncPriv(SocketPriv &&sock, DriverShared &driver, Handlers handlers)
  : SocketAsyncPriv(SocketBufferedPriv(std::move(sock), 0U, 0U),
                    driver,
                    std::move(handlers))
{
}

SocketAsyncPriv::SocketAsyncPriv(SocketBufferedPriv &&buff, DriverShared &driver, Handlers handlers)
  : SocketBufferedPriv(std::move(buff))
  , driver(driver)
  , handlers(std::move(handlers))
{
  driver->AsyncRegister(*this);

  if(this->handlers.disconnect) {
    // cache remote address as it will be unavailable after disconnect
    peerAddr = this->GetPeerName();
  }
}

SocketAsyncPriv::~SocketAsyncPriv()
{
  if(auto const ptr = driver.lock()) {
    ptr->AsyncUnregister(this->fd);
  }
}

std::future<void> SocketAsyncPriv::Send(BufferPtr &&buffer)
{
  return DoSend(sendQ, std::move(buffer));
}

std::future<void> SocketAsyncPriv::SendTo(BufferPtr &&buffer, AddressShared dstAddr)
{
  return DoSend(sendToQ, std::move(buffer), std::move(dstAddr));
}

template<typename Queue, typename... Args>
std::future<void> SocketAsyncPriv::DoSend(Queue &q, Args&&... args)
{
  std::promise<void> promise;
  auto ret = promise.get_future();

  bool wasEmpty;
  {
    std::lock_guard<std::mutex> lock(sendQMtx);

    wasEmpty = q.empty();
    q.emplace(std::move(promise),
              std::forward<Args>(args)...);
  }

  if(wasEmpty) {
    if(auto const ptr = driver.lock()) {
      ptr->AsyncWantSend(this->fd);
    }
  }

  return ret;
}

void SocketAsyncPriv::DriverDoFdTaskReadable()
try {
  if(handlers.connect) {
    auto [sock, addr] = this->Accept();
    this->Listen();

    handlers.connect(std::move(sock), std::move(addr));
  } else if(handlers.receive) {
    handlers.receive(SocketBufferedPriv::Receive());
  } else if(handlers.receiveFrom) {
    auto [buff, addr] = SocketBufferedPriv::ReceiveFrom();
    handlers.receiveFrom(std::move(buff), std::move(addr));
  } else {
    assert(false);
  }
} catch(std::runtime_error const &) {
  DriverDoFdTaskError();
}

bool SocketAsyncPriv::DriverDoFdTaskWritable()
{
  // hold the lock during send/sendto
  // as we already checked that the socket will not block and
  // otherwise we would need to re-lock afterwards to verify that
  // the previously empty queue has not been refilled asynchronously
  std::lock_guard<std::mutex> lock(sendQMtx);

  // one queue must have data but not both,
  // as socket uses either send or sendto
  assert(sendQ.empty() != sendToQ.empty());

  if(auto const sendQSize = sendQ.size()) {
    if(DriverDoSend(sendQ.front())) {
      sendQ.pop();
      return (sendQSize == 1U);
    }
  } else if(auto const sendToQSize = sendToQ.size()) {
    DriverDoSendTo(sendToQ.front());
    sendToQ.pop();
    return (sendToQSize == 1U);
  }
  return false;
}

bool SocketAsyncPriv::DriverDoSend(SendQElement &t)
{
  auto &&promise = std::get<0>(t);
  try {
    auto &&buffer = std::get<1>(t);

    // allow partial send to avoid starving other
    // driver's sockets if this one is rate limited
    auto const sent = SocketPriv::SendSome(buffer->data(), buffer->size());
    if(sent == buffer->size()) {
      promise.set_value();
      return true;
    } else {
      assert(sent > 0U);
      buffer->erase(0, sent);
    }
  } catch(std::exception const &e) {
    promise.set_exception(std::make_exception_ptr(e));
  }
  return false;
}

void SocketAsyncPriv::DriverDoSendTo(SendToQElement &t)
{
  auto &&promise = std::get<0>(t);
  try {
    auto &&buffer = std::get<1>(t);
    auto &&addr = std::get<2>(t);
    auto const sent = SocketPriv::SendTo(buffer->data(), buffer->size(),
                                         addr->ForUdp());
    assert(sent == buffer->size());
    promise.set_value();
  } catch(std::exception const &e) {
    promise.set_exception(std::make_exception_ptr(e));
  }
}

void SocketAsyncPriv::DriverDoFdTaskError()
{
  if(handlers.disconnect) {
    handlers.disconnect(Address(peerAddr));
  } else {
    // silently discard UDP receive errors
  }
}

} // namespace sockpuppet
