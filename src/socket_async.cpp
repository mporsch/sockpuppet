#include "socket_async.h"
#include "socket_address_priv.h" // for SocketAddress::SocketAddressPriv
#include "socket_async_priv.h" // for SocketAsyncPriv

namespace sockpuppet {

namespace {
  template<typename Handler>
  typename std::remove_reference<Handler>::type &&checkedMove(Handler &&handler)
  {
    if (!handler) {
      throw std::logic_error("invalid handler");
    }
    return std::move(handler);
  }
} // unnamed namespace

SocketDriver::SocketDriver()
  : m_priv(std::make_shared<SocketDriverPriv>())
{
}

SocketDriver::SocketDriver(SocketDriver &&other) noexcept
  : m_priv(std::move(other.m_priv))
{
}

SocketDriver::~SocketDriver() = default;

SocketDriver &SocketDriver::operator=(SocketDriver &&other) noexcept
{
  m_priv = std::move(other.m_priv);
  return *this;
}

void SocketDriver::Step(Duration timeout)
{
  m_priv->Step(timeout);
}

void SocketDriver::Run()
{
  m_priv->Run();
}

void SocketDriver::Stop()
{
  m_priv->Stop();
}


SocketAddress SocketAsync::LocalAddress() const
{
  return SocketAddress(m_priv->GetSockName());
}

size_t SocketAsync::ReceiveBufferSize() const
{
  auto const size = m_priv->GetSockOptRcvBuf();
  if(size < 0) {
    throw std::logic_error("invalid receive buffer size");
  }
  return static_cast<size_t>(size);
}

SocketAsync::SocketAsync(Socket &&sock,
    SocketDriver &driver, Handlers handlers)
  : m_priv(std::make_unique<SocketAsyncPriv>(
      std::move(*sock.m_priv), driver.m_priv, handlers))
{
}

SocketAsync::SocketAsync(SocketBuffered &&buff,
    SocketDriver &driver, Handlers handlers)
  : m_priv(std::make_unique<SocketAsyncPriv>(
      std::move(*buff.m_priv), driver.m_priv, handlers))
{
}

SocketAsync::SocketAsync(SocketAsync &&other) noexcept
  : m_priv(std::move(other.m_priv))
{
}

SocketAsync::~SocketAsync() = default;

SocketAsync &SocketAsync::operator=(SocketAsync &&other) noexcept
{
  m_priv = std::move(other.m_priv);
  return *this;
}


SocketUdpAsync::SocketUdpAsync(SocketUdpBuffered &&buff,
    SocketDriver &driver, ReceiveHandler handleReceive)
  : SocketAsync(std::move(buff),
                driver,
                Handlers{
                  checkedMove(handleReceive)
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
                , checkedMove(handleReceiveFrom)
                , nullptr
                , nullptr
                })
{
}

std::future<void> SocketUdpAsync::SendTo(SocketBufferPtr &&buffer,
    SocketAddress const &dstAddress)
{
  return m_priv->SendTo(std::move(buffer),
                        dstAddress.Priv().ForUdp());
}

SocketUdpAsync::SocketUdpAsync(SocketUdpAsync &&other) noexcept
  : SocketAsync(std::move(other))
{
}

SocketUdpAsync::~SocketUdpAsync() = default;

SocketUdpAsync &SocketUdpAsync::operator=(SocketUdpAsync &&other) noexcept
{
  SocketAsync::operator=(std::move(other));
  return *this;
}


SocketTcpAsyncClient::SocketTcpAsyncClient(
    SocketTcpBuffered &&buff, SocketDriver &driver,
    ReceiveHandler handleReceive, DisconnectHandler handleDisconnect)
  : SocketAsync(std::move(buff),
                driver,
                SocketAsync::Handlers{
                  checkedMove(handleReceive)
                , nullptr
                , nullptr
                , checkedMove(handleDisconnect)})
{
}

SocketTcpAsyncClient::SocketTcpAsyncClient(SocketTcpAsyncClient &&other) noexcept
  : SocketAsync(std::move(other))
{
}

SocketTcpAsyncClient::~SocketTcpAsyncClient() = default;

SocketTcpAsyncClient &SocketTcpAsyncClient::operator=(SocketTcpAsyncClient &&other) noexcept
{
  SocketAsync::operator=(std::move(other));
  return *this;
}

std::future<void> SocketTcpAsyncClient::Send(SocketBufferPtr &&buffer)
{
  return m_priv->Send(std::move(buffer));
}

SocketAddress SocketTcpAsyncClient::PeerAddress() const
{
  return SocketAddress(m_priv->GetPeerName());
}


SocketTcpAsyncServer::SocketTcpAsyncServer(SocketTcpServer &&sock,
    SocketDriver &driver, ConnectHandler handleConnect)
  : SocketAsync(std::move(sock),
                driver,
                SocketAsync::Handlers{
                  nullptr
                , nullptr
                , checkedMove(handleConnect)
                , nullptr})
{
  m_priv->Listen();
}

} // namespace sockpuppet
