#include "sockpuppet/socket_buffered.h"
#include "socket_buffered_priv.h" // for SocketBufferedPriv

namespace sockpuppet {

SocketAddress SocketBuffered::LocalAddress() const
{
  return SocketAddress(m_priv->GetSockName());
}

size_t SocketBuffered::ReceiveBufferSize() const
{
  return m_priv->GetSockOptRcvBuf();
}

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
  m_priv->SendTo(data, size, dstAddress.Priv().ForUdp());
}

SocketBuffered::SocketBufferPtr SocketUdpBuffered::Receive(Duration timeout)
{
  return m_priv->Receive(timeout);
}

std::tuple<SocketBuffered::SocketBufferPtr, SocketAddress>
SocketUdpBuffered::ReceiveFrom(Duration timeout)
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
    Duration timeout)
{
  m_priv->Send(data, size, timeout);
}

SocketBuffered::SocketBufferPtr SocketTcpBuffered::Receive(Duration timeout)
{
  return m_priv->Receive(timeout);
}

SocketAddress SocketTcpBuffered::PeerAddress() const
{
  return SocketAddress(m_priv->GetPeerName());
}

} // namespace sockpuppet
