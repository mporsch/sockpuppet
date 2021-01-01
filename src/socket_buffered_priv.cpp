#include "socket_buffered_priv.h"

namespace sockpuppet {

SocketBufferedPriv::SocketBufferedPriv(SocketPriv &&sock,
    size_t rxBufCount, size_t rxBufSize)
  : SocketPriv(std::move(sock))
  , pool(std::make_unique<BufferPool>(rxBufCount))
  , rxBufSize((rxBufSize ?
                 rxBufSize :
                 this->GetSockOptRcvBuf()))
{
}

SocketBufferedPriv::SocketBufferedPriv(SocketBufferedPriv &&other) noexcept = default;

SocketBufferedPriv::~SocketBufferedPriv() = default;

BufferPtr SocketBufferedPriv::GetBuffer()
{
  auto buffer = pool->Get();
  buffer->resize(rxBufSize);
  return buffer;
}

BufferPtr SocketBufferedPriv::Receive(Duration timeout)
{
  auto buffer = GetBuffer();

  buffer->resize(
    SocketPriv::Receive(
          const_cast<char *>(buffer->data()),
          buffer->size(),
          timeout));

  return buffer;
}

std::pair<BufferPtr, std::shared_ptr<SockAddrStorage>>
SocketBufferedPriv::ReceiveFrom(Duration timeout)
{
  auto buffer = GetBuffer();

  auto p = SocketPriv::ReceiveFrom(
        const_cast<char *>(buffer->data()),
        buffer->size(),
        timeout);
  buffer->resize(p.first);

  return {std::move(buffer), std::move(p.second)};
}

} // namespace sockpuppet
