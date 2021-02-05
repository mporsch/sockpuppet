#include "socket_async_priv.h"
#include "driver_priv.h" // for DriverPriv

#include <cassert> // for assert
#include <stdexcept> // for std::runtime_error

namespace sockpuppet {

namespace {
  auto const noTimeout = Duration(-1);
} // unnamed namespace

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
    auto p = this->Accept(noTimeout);
    this->Listen();

    handlers.connect(
          std::move(p.first),
          Address(std::move(p.second)));
  } else if(handlers.receive) {
    handlers.receive(this->Receive(noTimeout));
  } else if(handlers.receiveFrom) {
    auto p = this->ReceiveFrom(noTimeout);
    handlers.receiveFrom(
          std::move(p.first),
          Address(std::move(p.second)));
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

  // socket uses either send or sendto but not both
  assert(sendQ.empty() || sendToQ.empty());

  if(auto const sendQSize = sendQ.size()) {
    DriverDoSend(sendQ.front());
    sendQ.pop();
    return (sendQSize == 1U);
  } else if(auto const sendToQSize = sendToQ.size()) {
    DriverDoSendTo(sendToQ.front());
    sendToQ.pop();
    return (sendToQSize == 1U);
  }
  assert(false); // queue emptied unexpectedly
  return true;
}

void SocketAsyncPriv::DriverDoSend(SendQElement &t)
{
  auto &&promise = std::get<0>(t);
  try {
    auto &&buffer = std::get<1>(t);
    auto const sent = SocketPriv::Send(buffer->data(), buffer->size(), noTimeout);
    // as unlimited timeout is set, partially sent data is not expected
    assert(sent == buffer->size());
    promise.set_value();
  } catch(std::exception const &e) {
    promise.set_exception(std::make_exception_ptr(e));
  }
}

void SocketAsyncPriv::DriverDoSendTo(SendToQElement &t)
{
  auto &&promise = std::get<0>(t);
  try {
    auto &&buffer = std::get<1>(t);
    auto &&addr = std::get<2>(t);
    auto const sent = SocketPriv::SendTo(buffer->data(), buffer->size(),
                                         addr->ForUdp(),
                                         noTimeout);
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
