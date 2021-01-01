#ifndef SOCKPUPPET_SOCKET_BUFFERED_PRIV_H
#define SOCKPUPPET_SOCKET_BUFFERED_PRIV_H

#include "address_priv.h" // for SockAddrStorage
#include "socket_priv.h" // for SocketPriv
#include "sockpuppet/socket_buffered.h" // for BufferPool

#include <cstddef> // for size_t
#include <memory> // for std::unique_ptr
#include <utility> // for std::pair

namespace sockpuppet {

struct SocketBufferedPriv : public SocketPriv
{
  std::unique_ptr<BufferPool> pool;
  size_t rxBufSize;

  SocketBufferedPriv(SocketPriv &&sock,
                     size_t rxBufCount,
                     size_t rxBufSize);
  SocketBufferedPriv(SocketBufferedPriv const &) = delete;
  SocketBufferedPriv(SocketBufferedPriv &&other) noexcept;
  ~SocketBufferedPriv();

  BufferPtr GetBuffer();

  BufferPtr Receive(Duration timeout);

  std::pair<BufferPtr, std::shared_ptr<SockAddrStorage>>
  ReceiveFrom(Duration timeout);
};

} // namespace sockpuppet

#endif // SOCKPUPPET_SOCKET_BUFFERED_PRIV_H
