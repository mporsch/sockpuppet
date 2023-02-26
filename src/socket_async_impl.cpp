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
    ReceiveFromHandler onReceiveFrom)
  : buff(std::move(buff))
  , driver(driver)
  , onReadable(std::bind(
      &SocketAsyncImpl::DriverReceiveFrom,
      this,
      std::move(onReceiveFrom)))
  , onError([](char const *) {}) // silently discard UDP receive errors
  , sendQ(std::in_place_type<SendToQ>)
{
  driver->AsyncRegister(*this);
}

// TCP socket with Receive and Send
SocketAsyncImpl::SocketAsyncImpl(
    std::unique_ptr<SocketBufferedImpl> &&buff,
    DriverShared &driver,
    ReceiveHandler onReceive,
    DisconnectHandler onDisconnect)
  : buff(std::move(buff))
  , driver(driver)
  , onReadable(std::bind(
      &SocketAsyncImpl::DriverReceive,
      this,
      std::move(onReceive)))
  , onError(std::bind(
      &SocketAsyncImpl::DriverDisconnect,
      this,
      std::move(onDisconnect),
      this->buff->sock->GetPeerName(), // cache remote address now before disconnect
      std::placeholders::_1))
  , sendQ(std::in_place_type<SendQ>)
{
  driver->AsyncRegister(*this);
}

// TCP acceptor with Listen/Accept
SocketAsyncImpl::SocketAsyncImpl(
    std::unique_ptr<SocketImpl> &&sock,
    DriverShared &driver,
    ConnectHandler onConnect)
  : buff(std::make_unique<SocketBufferedImpl>(
      std::move(sock),
      0U, // no receive buffers needed
      1U)) // don't query SockOptRcvBuf
  , driver(driver)
  , onReadable(std::bind(
      &SocketAsyncImpl::DriverConnect,
      this,
      std::move(onConnect)))
  , onError([](char const *) {}) // silently discard TCP accept errors
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

  bool wasEmpty = DoSendEnqueue<Queue>(std::move(promise), std::forward<Args>(args)...);
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

void SocketAsyncImpl::DriverOnReadable()
{
  onReadable();
}

void SocketAsyncImpl::DriverConnect(ConnectHandler const &onConnect)
{
  try {
    auto [sock, addr] = buff->sock->Accept();
    buff->sock->Listen();

    onConnect(std::move(sock), std::move(addr));
  } catch(std::runtime_error const &e) {
    onError(e.what());
  }
}

void SocketAsyncImpl::DriverReceive(ReceiveHandler const &onReceive)
{
  if(pendingTlsSend) {
    pendingTlsSend = false;

    // a previous TLS send failed because handshake receipt was pending
    // which probably arrived now: repeat the same send call to handle
    // the handshake and continue where it left off sending
    // see https://www.openssl.org/docs/man1.1.1/man3/SSL_write.html
    bool isSendQueueEmpty = DriverOnWritable();
    if(!isSendQueueEmpty) {
      if(auto const ptr = driver.lock()) {
        ptr->AsyncWantSend(buff->sock->fd);
      }
    }

    return;
  }

  try {
    auto buffer = buff->Receive();
    if(buffer->empty()) {
      // TLS socket received handshake data only
    } else {
      onReceive(std::move(buffer));
    }
  } catch(std::runtime_error const &e) {
    onError(e.what());
  }
}

void SocketAsyncImpl::DriverReceiveFrom(ReceiveFromHandler const &onReceiveFrom)
{
  try {
    auto [buffer, addr] = buff->ReceiveFrom();
    onReceiveFrom(std::move(buffer), std::move(addr));
  } catch(std::runtime_error const &e) {
    onError(e.what());
  }
}

bool SocketAsyncImpl::DriverOnWritable()
{
  // hold the lock during send/sendto
  // as we already checked that the socket will not block and
  // otherwise we would need to re-lock afterwards to verify that
  // the previously empty queue has not been refilled asynchronously
  std::lock_guard<std::mutex> lock(sendQMtx);

  return std::visit([this](auto &&q) -> bool {
    using Q = std::decay_t<decltype(q)>;
    if constexpr(std::is_same_v<Q, SendQ>) {
      return DriverSend(q);
    } else if constexpr(std::is_same_v<Q, SendToQ>) {
      return DriverSendTo(q);
    }
  }, sendQ);
}

bool SocketAsyncImpl::DriverSend(SendQ &q)
{
  auto const sendQSize = q.size();
  if(!sendQSize) {
    throw std::logic_error("uncalled send");
  }

  auto &&[promise, buffer] = q.front();
  try {
    if(auto sent = buff->sock->SendSome(buffer->data(), buffer->size())) {
      if(sent == buffer->size()) {
        promise.set_value();
      } else {
        // allow partial send to avoid starving other driver's sockets if this one is rate limited
        buffer->erase(0, sent);
        return false;
      }
    } else { // zero-size sent data
      // TLS can't send while handshake receipt pending:
      // give up for now but keep the data in the send
      // queue and retry the exact same call on readable
      pendingTlsSend = true;
      return true;
    }
  } catch(std::runtime_error const &e) {
    promise.set_exception(std::make_exception_ptr(e));
  }
  q.pop();
  return (sendQSize == 1U);
}

bool SocketAsyncImpl::DriverSendTo(SendToQ &q)
{
  auto const sendToQSize = q.size();
  if(!sendToQSize) {
    throw std::logic_error("uncalled sendto");
  }

  auto &&[promise, buffer, addr] = q.front();
  try {
    [[maybe_unused]] auto sent = buff->sock->SendTo(
          buffer->data(), buffer->size(),
          addr->ForUdp());
    assert(sent == buffer->size());
    promise.set_value();
  } catch(std::runtime_error const &e) {
    promise.set_exception(std::make_exception_ptr(e));
  }
  q.pop();
  return (sendToQSize == 1U);
}

void SocketAsyncImpl::DriverOnError(char const *message)
{
  onError(message);
}

void SocketAsyncImpl::DriverDisconnect(DisconnectHandler const &onDisconnect,
    AddressShared peerAddr, char const *)
{
  onDisconnect(Address(std::move(peerAddr)));
}


} // namespace sockpuppet
