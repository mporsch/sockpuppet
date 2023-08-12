#include "socket_buffered_impl.h"

namespace sockpuppet {

SocketBufferedImpl::SocketBufferedImpl(std::unique_ptr<SocketImpl> &&sock,
    size_t rxBufCount, size_t rxBufSize)
  : sock(std::move(sock))
  , rxBufSize(rxBufSize ?
                rxBufSize :
                this->sock->GetSockOptRcvBuf())
  , pool(std::make_unique<BufferPool>(rxBufCount, this->rxBufSize))
{
}

SocketBufferedImpl::SocketBufferedImpl(SocketBufferedImpl &&other) noexcept = default;

SocketBufferedImpl::~SocketBufferedImpl() = default;

BufferPtr SocketBufferedImpl::GetBuffer()
{
  auto buffer = pool->Get();
  buffer->resize(rxBufSize);
  return buffer;
}

std::optional<BufferPtr> SocketBufferedImpl::Receive(Duration timeout)
{
  if(!WaitReadable(this->sock->fd, timeout)) {
    return {std::nullopt}; // timeout exceeded
  }
  return SocketBufferedImpl::Receive();
}

BufferPtr SocketBufferedImpl::Receive()
{
  auto buffer = GetBuffer();

  auto size = sock->Receive(
      const_cast<char *>(buffer->data()),
      buffer->size());
  buffer->resize(size);

  return buffer;
}

std::optional<std::pair<BufferPtr, Address>>
SocketBufferedImpl::ReceiveFrom(Duration timeout)
{
  if(!WaitReadable(this->sock->fd, timeout)) {
    return {std::nullopt}; // timeout exceeded
  }
  return SocketBufferedImpl::ReceiveFrom();
}

std::pair<BufferPtr, Address>
SocketBufferedImpl::ReceiveFrom()
{
  auto buffer = GetBuffer();

  auto [size, from] = sock->ReceiveFrom(
      const_cast<char *>(buffer->data()),
      buffer->size());
  buffer->resize(size);

  return {
    std::move(buffer),
    std::move(from)
  };
}

} // namespace sockpuppet
