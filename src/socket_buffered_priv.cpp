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

std::optional<BufferPtr> SocketBufferedPriv::Receive(Duration timeout)
{
  if(!this->WaitReadable(timeout)) {
    return {std::nullopt}; // timeout exceeded
  }
  return SocketBufferedPriv::Receive();
}

BufferPtr SocketBufferedPriv::Receive()
{
  auto buffer = GetBuffer();

  auto const size = SocketPriv::Receive(
      const_cast<char *>(buffer->data()),
      buffer->size());
  buffer->resize(size);

  return buffer;
}

std::optional<std::pair<BufferPtr, Address>>
SocketBufferedPriv::ReceiveFrom(Duration timeout)
{
  if(!this->WaitReadable(timeout)) {
    return {std::nullopt}; // timeout exceeded
  }
  return SocketBufferedPriv::ReceiveFrom();
}

std::pair<BufferPtr, Address>
SocketBufferedPriv::ReceiveFrom()
{
  auto buffer = GetBuffer();

  auto [size, from] = SocketPriv::ReceiveFrom(
      const_cast<char *>(buffer->data()),
      buffer->size());
  buffer->resize(size);

  return {std::move(buffer), std::move(from)};
}

} // namespace sockpuppet
