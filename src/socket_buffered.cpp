#include "socket_buffered.h"

SocketBuffered::SocketBuffered(size_t rxBufCount,
    size_t rxBufSize)
  : m_pool(std::make_unique<ResourcePool<SocketBuffer>>(rxBufCount))
  , m_rxBufSize(rxBufSize)
{
}

SocketBuffered::SocketBuffered(SocketBuffered &&other)
  : m_pool(std::move(other.m_pool))
  , m_rxBufSize(std::move(other.m_rxBufSize))
{
}

SocketBuffered::~SocketBuffered()
{
}

SocketBuffered &SocketBuffered::operator=(SocketBuffered &&other)
{
  m_pool = std::move(other.m_pool);
  m_rxBufSize = std::move(other.m_rxBufSize);
  return *this;
}

SocketBuffered::SocketBufferPtr SocketBuffered::GetBuffer()
{
  auto resource = m_pool->Get(m_rxBufSize);
  resource->resize(m_rxBufSize);
  return std::move(resource);
}


SocketUdpBuffered::SocketUdpBuffered(SocketUdp &&sock,
    size_t rxBufCount, size_t rxBufSize)
  : SocketUdp(std::move(sock))
  , SocketBuffered(rxBufCount,
                   (rxBufSize ?
                      rxBufSize :
                      Socket::GetReceiveBufferSize()))
{
}

SocketUdpBuffered::SocketBufferPtr SocketUdpBuffered::Receive(Time timeout)
{
  auto buffer = SocketBuffered::GetBuffer();

  buffer->resize(
    SocketUdp::Receive(
      buffer->data(), buffer->size(), timeout));

  return std::move(buffer);
}

std::tuple<SocketUdpBuffered::SocketBufferPtr, SocketAddress>
SocketUdpBuffered::ReceiveFrom(Time timeout)
{
  auto buffer = SocketBuffered::GetBuffer();

  auto t = SocketUdp::ReceiveFrom(
    buffer->data(), buffer->size(), timeout);
  buffer->resize(std::get<0>(t));

  return std::tuple<SocketUdpBuffered::SocketBufferPtr, SocketAddress>{
    std::move(buffer)
  , std::get<1>(t)
  };
}


SocketTcpBuffered::SocketTcpBuffered(SocketTcpClient &&sock,
    size_t rxBufCount, size_t rxBufSize)
  : SocketTcpClient(std::move(sock))
  , SocketBuffered(rxBufCount,
                   (rxBufSize ?
                      rxBufSize :
                      Socket::GetReceiveBufferSize()))
{
}

SocketBuffered::SocketBufferPtr SocketTcpBuffered::Receive(Time timeout)
{
  auto buffer = SocketBuffered::GetBuffer();

  buffer->resize(
    SocketTcpClient::Receive(
      buffer->data(), buffer->size(), timeout));

  return std::move(buffer);
}
