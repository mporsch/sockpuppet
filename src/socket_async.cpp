#include "sockpuppet/socket_async.h"
#include "socket_async_priv.h" // for SocketAsyncPriv

#include <stdexcept> // for std::logic_error

namespace sockpuppet {

namespace {
  template<typename Fn>
  std::function<Fn> &checked(std::function<Fn> &handler)
  {
    if(!handler) {
      throw std::logic_error("invalid handler");
    }
    return handler;
  }
} // unnamed namespace

SocketDriver::SocketDriver()
  : priv(std::make_shared<SocketDriverPriv>())
{
}


uint64_t SocketDriver::Schedule(Duration delay, std::function<void()> what)
{
  return priv->Schedule(delay, std::move(what));
}

void SocketDriver::Unschedule(uint64_t id)
{
  priv->Unschedule(id);
}

void SocketDriver::Reschedule(uint64_t id, Duration delay)
{
  priv->Reschedule(id, delay);
}

void SocketDriver::Step(Duration timeout)
{
  priv->Step(timeout);
}

void SocketDriver::Run()
{
  priv->Run();
}

void SocketDriver::Stop()
{
  priv->Stop();
}

SocketDriver::SocketDriver(SocketDriver &&other) noexcept = default;

SocketDriver::~SocketDriver() = default;

SocketDriver &SocketDriver::operator=(SocketDriver &&other) noexcept = default;


SocketUdpAsync::SocketUdpAsync(SocketUdpBuffered &&buff,
    SocketDriver &driver, ReceiveHandler handleReceive)
  : priv(std::make_unique<SocketAsyncPriv>(
      std::move(*buff.priv),
      driver.priv,
      SocketAsyncPriv::Handlers{
        std::move(checked(handleReceive))
      , nullptr
      , nullptr
      , nullptr
      }))
{
}

SocketUdpAsync::SocketUdpAsync(SocketUdpBuffered &&buff,
    SocketDriver &driver, ReceiveFromHandler handleReceiveFrom)
  : priv(std::make_unique<SocketAsyncPriv>(
      std::move(*buff.priv),
      driver.priv,
      SocketAsyncPriv::Handlers{
        nullptr
      , std::move(checked(handleReceiveFrom))
      , nullptr
      , nullptr
      }))
{
}

std::future<void> SocketUdpAsync::SendTo(BufferPtr &&buffer,
    Address const &dstAddress)
{
  return priv->SendTo(std::move(buffer), dstAddress.priv);
}

Address SocketUdpAsync::LocalAddress() const
{
  return Address(priv->GetSockName());
}

SocketUdpAsync::SocketUdpAsync(SocketUdpAsync &&other) noexcept = default;

SocketUdpAsync::~SocketUdpAsync() = default;

SocketUdpAsync &SocketUdpAsync::operator=(SocketUdpAsync &&other) noexcept = default;


SocketTcpAsyncClient::SocketTcpAsyncClient(
    SocketTcpBuffered &&buff, SocketDriver &driver,
    ReceiveHandler handleReceive, DisconnectHandler handleDisconnect)
  : priv(std::make_unique<SocketAsyncPriv>(
      std::move(*buff.priv),
      driver.priv,
      SocketAsyncPriv::Handlers{
        std::move(checked(handleReceive))
      , nullptr
      , nullptr
      , std::move(checked(handleDisconnect))
      }))
{
}

std::future<void> SocketTcpAsyncClient::Send(BufferPtr &&buffer)
{
  return priv->Send(std::move(buffer));
}

Address SocketTcpAsyncClient::LocalAddress() const
{
  return Address(priv->GetSockName());
}

Address SocketTcpAsyncClient::PeerAddress() const
{
  return Address(priv->GetPeerName());
}

SocketTcpAsyncClient::SocketTcpAsyncClient(SocketTcpAsyncClient &&other) noexcept = default;

SocketTcpAsyncClient::~SocketTcpAsyncClient() = default;

SocketTcpAsyncClient &SocketTcpAsyncClient::operator=(SocketTcpAsyncClient &&other) noexcept = default;


SocketTcpAsyncServer::SocketTcpAsyncServer(SocketTcpServer &&sock,
    SocketDriver &driver, ConnectHandler handleConnect)
  : priv(std::make_unique<SocketAsyncPriv>(
      std::move(*sock.priv),
      driver.priv,
      SocketAsyncPriv::Handlers{
        nullptr
      , nullptr
      , std::move(checked(handleConnect))
      , nullptr
      }))
{
  priv->Listen();
}

Address SocketTcpAsyncServer::LocalAddress() const
{
  return Address(priv->GetSockName());
}

SocketTcpAsyncServer::SocketTcpAsyncServer(SocketTcpAsyncServer &&other) noexcept = default;

SocketTcpAsyncServer::~SocketTcpAsyncServer() = default;

SocketTcpAsyncServer &SocketTcpAsyncServer::operator=(SocketTcpAsyncServer &&other) noexcept = default;

} // namespace sockpuppet
