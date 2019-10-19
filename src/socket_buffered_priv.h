#ifndef SOCKPUPPET_SOCKET_BUFFERED_PRIV_H
#define SOCKPUPPET_SOCKET_BUFFERED_PRIV_H

#include "sockpuppet/socket_buffered.h" // for SocketBuffered
#include "socket_priv.h" // for Socket::SocketPriv

#include <cstddef> // for size_t
#include <memory> // for std::unique_ptr

namespace sockpuppet {

struct SocketBuffered::SocketBufferedPriv : public Socket::SocketPriv
{
  std::unique_ptr<SocketBufferPool> pool;
  size_t rxBufSize;

  SocketBufferedPriv(SocketPriv &&sock,
                     size_t rxBufCount,
                     size_t rxBufSize);
  SocketBufferedPriv(SocketBufferedPriv const &) = delete;
  SocketBufferedPriv(SocketBufferedPriv &&other) noexcept;
  ~SocketBufferedPriv() override;

  SocketBufferPtr GetBuffer();

  SocketBufferPtr Receive(Duration timeout);
  std::tuple<SocketBufferPtr, Address> ReceiveFrom(Duration timeout);
};

} // namespace sockpuppet

#endif // SOCKPUPPET_SOCKET_BUFFERED_PRIV_H
