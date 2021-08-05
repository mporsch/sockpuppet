#include "socket_buffered_priv.h"
#include "wait.h" // for WaitReadableBlocking

namespace sockpuppet {

SocketBufferedPriv::SocketBufferedPriv(std::unique_ptr<SocketPriv> &&sock,
    size_t rxBufCount, size_t rxBufSize)
  : sock(std::move(sock))
  , pool(std::make_unique<BufferPool>(rxBufCount))
  , rxBufSize((rxBufSize ?
                 rxBufSize :
                 this->sock->GetSockOptRcvBuf()))
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
  if(!WaitReadableBlocking(sock->fd, timeout)) {
    return {std::nullopt}; // timeout exceeded
  }
  return SocketBufferedPriv::Receive();
}

BufferPtr SocketBufferedPriv::Receive()
{
  auto buffer = GetBuffer();

  auto const size = sock->Receive(
      const_cast<char *>(buffer->data()),
      buffer->size());
  buffer->resize(size);

  return buffer;
}

std::optional<std::pair<BufferPtr, Address>>
SocketBufferedPriv::ReceiveFrom(Duration timeout)
{
  if(!WaitReadableBlocking(sock->fd, timeout)) {
    return {std::nullopt}; // timeout exceeded
  }
  return SocketBufferedPriv::ReceiveFrom();
}

std::pair<BufferPtr, Address>
SocketBufferedPriv::ReceiveFrom()
{
  auto buffer = GetBuffer();

  auto [size, from] = sock->ReceiveFrom(
      const_cast<char *>(buffer->data()),
      buffer->size());
  buffer->resize(size);

  return {std::move(buffer), std::move(from)};
}

} // namespace sockpuppet
