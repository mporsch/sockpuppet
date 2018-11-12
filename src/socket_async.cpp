#include "socket_async.h"
#include "socket_address_priv.h" // for SocketAddress::SocketAddressPriv
#include "socket_async_priv.h" // for SocketAsyncPriv

SocketDriver::SocketDriver()
  : m_priv(std::make_shared<SocketDriverPriv>())
{
}

SocketDriver::SocketDriver(SocketDriver &&other)
  : m_priv(std::move(other.m_priv))
{
}

SocketDriver::~SocketDriver()
{
}

SocketDriver &SocketDriver::operator=(SocketDriver &&other)
{
  m_priv = std::move(other.m_priv);
  return *this;
}

void SocketDriver::Step()
{
  m_priv->Step();
}

void SocketDriver::Run()
{
  m_priv->Run();
}

void SocketDriver::Stop()
{
  m_priv->Stop();
}


SocketAsync::SocketAsync(Socket &&sock,
    SocketDriver &driver,
    Handlers handlers)
  : m_priv(std::make_unique<SocketAsyncPriv>(
      std::move(*sock.m_priv), driver.m_priv, handlers))
{
}

SocketAsync::SocketAsync(SocketBuffered &&buff,
    SocketDriver &driver,
    Handlers handlers)
  : m_priv(std::make_unique<SocketAsyncPriv>(
      std::move(*buff.m_priv), driver.m_priv, handlers))
{
}

SocketAsync::SocketAsync(SocketAsync &&other)
  : m_priv(std::move(other.m_priv))
{
}

SocketAsync::~SocketAsync()
{
}

SocketAsync &SocketAsync::operator=(SocketAsync &&other)
{
  m_priv = std::move(other.m_priv);
  return *this;
}


SocketUdpAsync::SocketUdpAsync(SocketUdpBuffered &&buff,
    SocketDriver &driver,
    ReceiveHandler handleReceive,
    ReceiveFromHandler handleReceiveFrom)
  : SocketAsync(std::move(buff),
                driver,
                Handlers{
                  handleReceive
                , handleReceiveFrom
                , nullptr
                , nullptr
                })
{
}

std::future<void> SocketUdpAsync::SendTo(SocketBufferPtr &&buffer,
  SocketAddress const &dstAddress)
{
  return m_priv->SendTo(std::move(buffer),
                        dstAddress.Priv()->SockAddrUdp());
}

SocketUdpAsync::SocketUdpAsync(SocketUdpAsync &&other)
  : SocketAsync(std::move(other))
{
}

SocketUdpAsync &SocketUdpAsync::operator=(SocketUdpAsync &&other)
{
  SocketAsync::operator=(std::move(other));
  return *this;
}


SocketTcpAsyncClient::SocketTcpAsyncClient(
    SocketTcpBuffered &&buff,
    SocketDriver &driver,
    ReceiveHandler handleReceive,
    DisconnectHandler handleDisconnect)
  : SocketAsync(std::move(buff),
                driver,
                SocketAsync::Handlers{
                  handleReceive
                , nullptr
                , nullptr
                , handleDisconnect})
{
}

SocketTcpAsyncClient::SocketTcpAsyncClient(SocketTcpAsyncClient &&other)
  : SocketAsync(std::move(other))
{
}

SocketTcpAsyncClient &SocketTcpAsyncClient::operator=(SocketTcpAsyncClient &&other)
{
  SocketAsync::operator=(std::move(other));
  return *this;
}

std::future<void> SocketTcpAsyncClient::Send(SocketBufferPtr &&buffer)
{
  return m_priv->Send(std::move(buffer));
}


SocketTcpAsyncServer::SocketTcpAsyncServer(
    SocketTcpServer &&sock,
    SocketDriver &driver,
    ConnectHandler handleConnect)
  : SocketAsync(std::move(sock),
                driver,
                SocketAsync::Handlers{
                  nullptr
                , nullptr
                , handleConnect
                , nullptr})
{
  m_priv->Listen();
}
