#ifndef SOCKPUPPET_SOCKET_BUFFERED_IMPL_H
#define SOCKPUPPET_SOCKET_BUFFERED_IMPL_H

#include "socket_impl.h" // for SocketImpl
#include "sockpuppet/address.h" // for Address
#include "sockpuppet/socket_buffered.h" // for BufferPool

#include <cstddef> // for size_t
#include <memory> // for std::unique_ptr
#include <optional> // for std::optional
#include <utility> // for std::pair

namespace sockpuppet {

struct SocketBufferedImpl
{
  std::unique_ptr<SocketImpl> sock;
  std::unique_ptr<BufferPool> pool;
  size_t rxBufSize;

  SocketBufferedImpl(std::unique_ptr<SocketImpl> &&sock,
                     size_t rxBufCount,
                     size_t rxBufSize);
  SocketBufferedImpl(SocketBufferedImpl const &) = delete;
  SocketBufferedImpl(SocketBufferedImpl &&other) noexcept;
  ~SocketBufferedImpl();

  BufferPtr GetBuffer();

  std::optional<BufferPtr> Receive(Duration timeout);
  BufferPtr Receive();

  std::optional<std::pair<BufferPtr, Address>>
  ReceiveFrom(Duration timeout);
  std::pair<BufferPtr, Address>
  ReceiveFrom();
};

} // namespace sockpuppet

#endif // SOCKPUPPET_SOCKET_BUFFERED_IMPL_H
