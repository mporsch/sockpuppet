#include "socket_buffered.h"
#include "socket_buffered_priv.h" // for SocketBufferedPriv

namespace sockpuppet {

SocketBuffered::SocketBuffered(Socket &&sock,
    size_t rxBufCount, size_t rxBufSize)
  : m_priv(std::make_unique<SocketBufferedPriv>(
      std::move(*sock.m_priv), rxBufCount, rxBufSize))
{
}

SocketBuffered::SocketBuffered(SocketBuffered &&other) noexcept
  : m_priv(std::move(other.m_priv))
{
}

SocketBuffered::~SocketBuffered() = default;

SocketBuffered &SocketBuffered::operator=(SocketBuffered &&other) noexcept
{
  m_priv = std::move(other.m_priv);
  return *this;
}


SocketUdpBuffered::SocketUdpBuffered(SocketUdp &&sock,
    size_t rxBufCount, size_t rxBufSize)
  : SocketBuffered(std::move(sock), rxBufCount, rxBufSize)
{
}

SocketUdpBuffered::SocketUdpBuffered(SocketUdpBuffered &&other) noexcept
  : SocketBuffered(std::move(other))
{
}

SocketUdpBuffered::~SocketUdpBuffered() = default;

SocketUdpBuffered &SocketUdpBuffered::operator=(SocketUdpBuffered &&other) noexcept
{
  SocketBuffered::operator=(std::move(other));
  return *this;
}

void SocketUdpBuffered::SendTo(char const *data, size_t size,
    SocketAddress const &dstAddress)
{
  m_priv->SendTo(data, size, dstAddress.Priv().SockAddrUdp());
}

SocketBuffered::SocketBufferPtr SocketUdpBuffered::Receive(Time timeout)
{
  return m_priv->Receive(timeout);
}

std::tuple<SocketBuffered::SocketBufferPtr, SocketAddress>
SocketUdpBuffered::ReceiveFrom(Time timeout)
{
  return m_priv->ReceiveFrom(timeout);
}


SocketTcpBuffered::SocketTcpBuffered(SocketTcpClient &&sock,
    size_t rxBufCount, size_t rxBufSize)
  : SocketBuffered(std::move(sock), rxBufCount, rxBufSize)
{
}

SocketTcpBuffered::SocketTcpBuffered(SocketTcpBuffered &&other) noexcept
  : SocketBuffered(std::move(other))
{
}

SocketTcpBuffered::~SocketTcpBuffered() = default;

SocketTcpBuffered &SocketTcpBuffered::operator=(SocketTcpBuffered &&other) noexcept
{
  SocketBuffered::operator=(std::move(other));
  return *this;
}

void SocketTcpBuffered::Send(char const *data, size_t size,
    Time timeout)
{
  m_priv->Send(data, size, timeout);
}

SocketBuffered::SocketBufferPtr SocketTcpBuffered::Receive(Time timeout)
{
  return m_priv->Receive(timeout);
}

} // namespace sockpuppet
