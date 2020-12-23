#include "sockpuppet/socket_async.h"
#include "address_priv.h" // for Address::AddressPriv
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

SocketDriver::SocketDriver(SocketDriver &&other) noexcept = default;

SocketDriver::~SocketDriver() = default;

SocketDriver &SocketDriver::operator=(SocketDriver &&other) noexcept = default;

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


Address SocketAsync::LocalAddress() const
{
  return Address(priv->GetSockName());
}

size_t SocketAsync::ReceiveBufferSize() const
{
  return priv->GetSockOptRcvBuf();
}

SocketAsync::SocketAsync(Socket &&sock,
    SocketDriver &driver, Handlers handlers)
  : priv(std::make_unique<SocketAsyncPriv>(
      std::move(*sock.priv), driver.priv, std::move(handlers)))
{
}

SocketAsync::SocketAsync(SocketBuffered &&buff,
    SocketDriver &driver, Handlers handlers)
  : priv(std::make_unique<SocketAsyncPriv>(
      std::move(*buff.priv), driver.priv, std::move(handlers)))
{
}

SocketAsync::SocketAsync(SocketAsync &&other) noexcept = default;

SocketAsync::~SocketAsync() = default;

SocketAsync &SocketAsync::operator=(SocketAsync &&other) noexcept = default;


SocketUdpAsync::SocketUdpAsync(SocketUdpBuffered &&buff,
    SocketDriver &driver, ReceiveHandler handleReceive)
  : SocketAsync(std::move(buff),
                driver,
                Handlers{
                  std::move(checked(handleReceive))
                , nullptr
                , nullptr
                , nullptr
                })
{
}

SocketUdpAsync::SocketUdpAsync(SocketUdpBuffered &&buff,
    SocketDriver &driver, ReceiveFromHandler handleReceiveFrom)
  : SocketAsync(std::move(buff),
                driver,
                Handlers{
                  nullptr
                , std::move(checked(handleReceiveFrom))
                , nullptr
                , nullptr
                })
{
}

std::future<void> SocketUdpAsync::SendTo(BufferPtr &&buffer,
    Address const &dstAddress)
{
  return priv->SendTo(std::move(buffer), dstAddress);
}

SocketUdpAsync::SocketUdpAsync(SocketUdpAsync &&other) noexcept = default;

SocketUdpAsync::~SocketUdpAsync() = default;

SocketUdpAsync &SocketUdpAsync::operator=(SocketUdpAsync &&other) noexcept = default;


SocketTcpAsyncClient::SocketTcpAsyncClient(
    SocketTcpBuffered &&buff, SocketDriver &driver,
    ReceiveHandler handleReceive, DisconnectHandler handleDisconnect)
  : SocketAsync(std::move(buff),
                driver,
                SocketAsync::Handlers{
                  std::move(checked(handleReceive))
                , nullptr
                , nullptr
                , std::move(checked(handleDisconnect))})
{
}

SocketTcpAsyncClient::SocketTcpAsyncClient(SocketTcpAsyncClient &&other) noexcept = default;

SocketTcpAsyncClient::~SocketTcpAsyncClient() = default;

SocketTcpAsyncClient &SocketTcpAsyncClient::operator=(SocketTcpAsyncClient &&other) noexcept = default;

std::future<void> SocketTcpAsyncClient::Send(BufferPtr &&buffer)
{
  return priv->Send(std::move(buffer));
}

Address SocketTcpAsyncClient::PeerAddress() const
{
  return Address(priv->GetPeerName());
}


SocketTcpAsyncServer::SocketTcpAsyncServer(SocketTcpServer &&sock,
    SocketDriver &driver, ConnectHandler handleConnect)
  : SocketAsync(std::move(sock),
                driver,
                SocketAsync::Handlers{
                  nullptr
                , nullptr
                , std::move(checked(handleConnect))
                , nullptr})
{
  priv->Listen();
}

SocketTcpAsyncServer::SocketTcpAsyncServer(SocketTcpAsyncServer &&other) noexcept = default;

SocketTcpAsyncServer::~SocketTcpAsyncServer() = default;

SocketTcpAsyncServer &SocketTcpAsyncServer::operator=(SocketTcpAsyncServer &&other) noexcept = default;

} // namespace sockpuppet
