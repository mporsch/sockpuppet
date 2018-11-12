#include "socket_buffered_priv.h"

SocketBuffered::SocketBufferedPriv::SocketBufferedPriv(Socket::SocketPriv &&sock,
    size_t rxBufCount, size_t rxBufSize)
  : Socket::SocketPriv(std::move(sock))
  , pool(std::make_unique<ResourcePool<SocketBuffer>>(rxBufCount))
  , rxBufSize((rxBufSize ?
                 rxBufSize :
                 this->GetSockOptRcvBuf()))
{
}

SocketBuffered::SocketBufferedPriv::SocketBufferedPriv(SocketBufferedPriv &&other)
  : Socket::SocketPriv(std::move(other))
  , pool(std::move(other.pool))
  , rxBufSize(other.rxBufSize)
{
}

SocketBuffered::SocketBufferedPriv::~SocketBufferedPriv()
{
}

SocketBuffered::SocketBufferPtr SocketBuffered::SocketBufferedPriv::GetBuffer()
{
  auto resource = pool->Get(rxBufSize);
  resource->resize(rxBufSize);
  return std::move(resource);
}

SocketBuffered::SocketBufferPtr
SocketBuffered::SocketBufferedPriv::Receive(Socket::Time timeout)
{
  auto buffer = GetBuffer();

  buffer->resize(
    SocketPriv::Receive(
      buffer->data(), buffer->size(), timeout));

  return std::move(buffer);
}

std::tuple<SocketBuffered::SocketBufferPtr, SocketAddress>
SocketBuffered::SocketBufferedPriv::ReceiveFrom(Socket::Time timeout)
{
  auto buffer = GetBuffer();

  auto t = SocketPriv::ReceiveFrom(
    buffer->data(), buffer->size(), timeout);
  buffer->resize(std::get<0>(t));

  return std::tuple<SocketUdpBuffered::SocketBufferPtr, SocketAddress>{
    std::move(buffer)
  , SocketAddress(std::move(std::get<1>(t)))
  };
}
