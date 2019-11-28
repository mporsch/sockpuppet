#include "socket_buffered_priv.h"

namespace sockpuppet {

SocketBuffered::SocketBufferedPriv::SocketBufferedPriv(SocketPriv &&sock,
    size_t rxBufCount, size_t rxBufSize)
  : SocketPriv(std::move(sock))
  , pool(std::make_unique<BufferPool>(rxBufCount))
  , rxBufSize((rxBufSize ?
                 rxBufSize :
                 this->GetSockOptRcvBuf()))
{
}

SocketBuffered::SocketBufferedPriv::SocketBufferedPriv(SocketBufferedPriv &&other) noexcept = default;

SocketBuffered::SocketBufferedPriv::~SocketBufferedPriv() = default;

BufferPtr SocketBuffered::SocketBufferedPriv::GetBuffer()
{
  auto buffer = pool->Get();
  buffer->resize(rxBufSize);
  return buffer;
}

BufferPtr SocketBuffered::SocketBufferedPriv::Receive(Duration timeout)
{
  auto buffer = GetBuffer();

  buffer->resize(
    SocketPriv::Receive(
      buffer->data(), buffer->size(), timeout));

  return buffer;
}

std::pair<BufferPtr, Address>
SocketBuffered::SocketBufferedPriv::ReceiveFrom(Duration timeout)
{
  auto buffer = GetBuffer();

  auto p = SocketPriv::ReceiveFrom(
    buffer->data(), buffer->size(), timeout);
  buffer->resize(p.first);

  return {std::move(buffer), Address(std::move(p.second))};
}

} // namespace sockpuppet
