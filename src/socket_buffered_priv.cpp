#include "socket_buffered_priv.h"

namespace sockpuppet {

SocketBuffered::SocketBufferedPriv::SocketBufferedPriv(SocketPriv &&sock,
    size_t rxBufCount, size_t rxBufSize)
  : SocketPriv(std::move(sock))
  , pool(std::make_unique<ResourcePool<SocketBuffer>>(rxBufCount))
  , rxBufSize((rxBufSize ?
                 rxBufSize :
                 this->GetSockOptRcvBuf()))
{
}

SocketBuffered::SocketBufferedPriv::SocketBufferedPriv(SocketBufferedPriv &&other) noexcept = default;

SocketBuffered::SocketBufferedPriv::~SocketBufferedPriv() = default;

SocketBuffered::SocketBufferPtr SocketBuffered::SocketBufferedPriv::GetBuffer()
{
  auto buffer = pool->Get(rxBufSize);

  buffer->resize(rxBufSize);

  return buffer;
}

SocketBuffered::SocketBufferPtr
SocketBuffered::SocketBufferedPriv::Receive(Duration timeout)
{
  auto buffer = GetBuffer();

  buffer->resize(
    SocketPriv::Receive(
      buffer->data(), buffer->size(), timeout));

  return buffer;
}

std::pair<SocketBuffered::SocketBufferPtr, Address>
SocketBuffered::SocketBufferedPriv::ReceiveFrom(Duration timeout)
{
  auto buffer = GetBuffer();

  auto p = SocketPriv::ReceiveFrom(
    buffer->data(), buffer->size(), timeout);
  buffer->resize(p.first);

  return std::pair<SocketBufferPtr, Address>(
        std::move(buffer), std::move(p.second));
}

} // namespace sockpuppet
