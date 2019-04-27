#include "socket_buffered_priv.h"

SocketBuffered::SocketBufferedPriv::SocketBufferedPriv(SocketPriv &&sock,
    size_t rxBufCount, size_t rxBufSize)
  : SocketPriv(std::move(sock))
  , pool(std::make_unique<ResourcePool<SocketBuffer>>(rxBufCount))
  , rxBufSize((rxBufSize ?
                 rxBufSize :
                 this->GetSockOptRcvBuf()))
{
}

SocketBuffered::SocketBufferedPriv::SocketBufferedPriv(SocketBufferedPriv &&other) noexcept
  : SocketPriv(std::move(other))
  , pool(std::move(other.pool))
  , rxBufSize(other.rxBufSize)
{
}

SocketBuffered::SocketBufferedPriv::~SocketBufferedPriv() = default;

SocketBuffered::SocketBufferPtr SocketBuffered::SocketBufferedPriv::GetBuffer()
{
  auto buffer = pool->Get(rxBufSize);

  buffer->resize(rxBufSize);

  return buffer;
}

SocketBuffered::SocketBufferPtr
SocketBuffered::SocketBufferedPriv::Receive(Time timeout)
{
  auto buffer = GetBuffer();

  buffer->resize(
    SocketPriv::Receive(
      buffer->data(), buffer->size(), timeout));

  return buffer;
}

std::tuple<SocketBuffered::SocketBufferPtr, SocketAddress>
SocketBuffered::SocketBufferedPriv::ReceiveFrom(Time timeout)
{
  auto buffer = GetBuffer();

  auto t = SocketPriv::ReceiveFrom(
    buffer->data(), buffer->size(), timeout);
  buffer->resize(std::get<0>(t));

  return std::tuple<SocketBufferPtr, SocketAddress>{
    std::move(buffer)
  , SocketAddress(std::move(std::get<1>(t)))
  };
}
