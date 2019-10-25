#include "sockpuppet/socket_buffered.h"
#include "socket_buffered_priv.h" // for SocketBufferedPriv

namespace sockpuppet {

Address SocketBuffered::LocalAddress() const
{
  return Address(m_priv->GetSockName());
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

SocketBuffered::SocketBuffered(SocketBuffered &&other) noexcept = default;

SocketBuffered::~SocketBuffered() = default;

SocketBuffered &SocketBuffered::operator=(SocketBuffered &&other) noexcept = default;


SocketUdpBuffered::SocketUdpBuffered(SocketUdp &&sock,
    size_t rxBufCount, size_t rxBufSize)
  : SocketBuffered(std::move(sock), rxBufCount, rxBufSize)
{
}

SocketUdpBuffered::SocketUdpBuffered(SocketUdpBuffered &&other) noexcept = default;

SocketUdpBuffered::~SocketUdpBuffered() = default;

SocketUdpBuffered &SocketUdpBuffered::operator=(SocketUdpBuffered &&other) noexcept = default;

size_t SocketUdpBuffered::SendTo(char const *data, size_t size,
    Address const &dstAddress, Duration timeout)
{
  return m_priv->SendTo(data, size, dstAddress.Priv().ForUdp(), timeout);
}

SocketBuffered::SocketBufferPtr SocketUdpBuffered::Receive(Duration timeout)
{
  return m_priv->Receive(timeout);
}

std::pair<SocketBuffered::SocketBufferPtr, Address>
SocketUdpBuffered::ReceiveFrom(Duration timeout)
{
  return m_priv->ReceiveFrom(timeout);
}


SocketTcpBuffered::SocketTcpBuffered(SocketTcpClient &&sock,
    size_t rxBufCount, size_t rxBufSize)
  : SocketBuffered(std::move(sock), rxBufCount, rxBufSize)
{
}

SocketTcpBuffered::SocketTcpBuffered(SocketTcpBuffered &&other) noexcept = default;

SocketTcpBuffered::~SocketTcpBuffered() = default;

SocketTcpBuffered &SocketTcpBuffered::operator=(SocketTcpBuffered &&other) noexcept = default;

size_t SocketTcpBuffered::Send(char const *data, size_t size,
    Duration timeout)
{
  return m_priv->Send(data, size, timeout);
}

SocketBuffered::SocketBufferPtr SocketTcpBuffered::Receive(Duration timeout)
{
  return m_priv->Receive(timeout);
}

Address SocketTcpBuffered::PeerAddress() const
{
  return Address(m_priv->GetPeerName());
}

} // namespace sockpuppet
