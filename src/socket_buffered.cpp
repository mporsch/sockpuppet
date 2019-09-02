#include "socket_buffered.h"
#include "socket_buffered_priv.h" // for SocketBufferedPriv

namespace sockpuppet {

SocketAddress SocketBuffered::LocalAddress() const
{
  return SocketAddress(m_priv->GetSockName());
}

size_t SocketBuffered::ReceiveBufferSize() const
{
  auto const size = m_priv->GetSockOptRcvBuf();
  if(size < 0) {
    throw std::logic_error("invalid receive buffer size");
  }
  return static_cast<size_t>(size);
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

SocketAddress SocketTcpBuffered::PeerAddress() const
{
  return SocketAddress(m_priv->GetPeerName());
}

} // namespace sockpuppet
