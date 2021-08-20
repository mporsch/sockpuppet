#include "socket_async_impl.h"
#include "driver_impl.h" // for DriverImpl

#include <cassert> // for assert
#include <stdexcept> // for std::runtime_error

namespace sockpuppet {

SocketAsyncImpl::SocketAsyncImpl(std::unique_ptr<SocketImpl> &&sock,
    DriverShared &driver, Handlers handlers)
  : SocketAsyncImpl(std::make_unique<SocketBufferedImpl>(std::move(sock), 0U, 0U),
                    driver,
                    std::move(handlers))
{
}

SocketAsyncImpl::SocketAsyncImpl(std::unique_ptr<SocketBufferedImpl> &&buff,
    DriverShared &driver, Handlers handlers)
  : buff(std::move(buff))
  , driver(driver)
  , handlers(std::move(handlers))
{
  driver->AsyncRegister(*this);

  if(this->handlers.disconnect) {
    // cache remote address as it will be unavailable after disconnect
    peerAddr = this->buff->sock->GetPeerName();
  }
}

SocketAsyncImpl::~SocketAsyncImpl()
{
  if(auto const ptr = driver.lock()) {
    ptr->AsyncUnregister(buff->sock->fd);
  }
}

std::future<void> SocketAsyncImpl::Send(BufferPtr &&buffer)
{
  return DoSend(sendQ, std::move(buffer));
}

std::future<void> SocketAsyncImpl::SendTo(BufferPtr &&buffer, AddressShared dstAddr)
{
  return DoSend(sendToQ, std::move(buffer), std::move(dstAddr));
}

template<typename Queue, typename... Args>
std::future<void> SocketAsyncImpl::DoSend(Queue &q, Args&&... args)
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
      ptr->AsyncWantSend(buff->sock->fd);
    }
  }

  return ret;
}

void SocketAsyncImpl::DriverDoFdTaskReadable()
try {
  if(handlers.connect) {
    auto [sock, addr] = buff->sock->Accept();
    buff->sock->Listen();

    handlers.connect(std::move(sock), std::move(addr));
  } else if(handlers.receive) {
      auto buffer = buff->Receive();
      if(!buffer->empty()) { // TLS socket received handshake data only
        handlers.receive(std::move(buffer));
      }
  } else if(handlers.receiveFrom) {
    auto [buffer, addr] = buff->ReceiveFrom();
    handlers.receiveFrom(std::move(buffer), std::move(addr));
  } else {
    assert(false);
  }
} catch(std::runtime_error const &) {
  DriverDoFdTaskError();
}

bool SocketAsyncImpl::DriverDoFdTaskWritable()
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

bool SocketAsyncImpl::DriverDoSend(SendQElement &t)
{
  auto &&promise = std::get<0>(t);
  try {
    auto &&buffer = std::get<1>(t);

    // allow partial send to avoid starving other
    // driver's sockets if this one is rate limited
    auto const sent = buff->sock->SendSome(buffer->data(), buffer->size());
    if(sent == buffer->size()) {
      promise.set_value();
    } else {
      buffer->erase(0, sent);
      return false;
    }
  } catch(std::exception const &e) {
    promise.set_exception(std::make_exception_ptr(e));
  }
  return true;
}

void SocketAsyncImpl::DriverDoSendTo(SendToQElement &t)
{
  auto &&promise = std::get<0>(t);
  try {
    auto &&buffer = std::get<1>(t);
    auto &&addr = std::get<2>(t);
    auto const sent = buff->sock->SendTo(buffer->data(), buffer->size(),
                                         addr->ForUdp());
    assert(sent == buffer->size());
    promise.set_value();
  } catch(std::exception const &e) {
    promise.set_exception(std::make_exception_ptr(e));
  }
}

void SocketAsyncImpl::DriverDoFdTaskError()
{
  if(handlers.disconnect) {
    handlers.disconnect(Address(peerAddr));
  } else {
    // silently discard UDP receive errors
  }
}

} // namespace sockpuppet