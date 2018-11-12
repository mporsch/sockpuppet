#ifndef SOCKET_BUFFERED_PRIV_H
#define SOCKET_BUFFERED_PRIV_H

#include "socket_buffered.h" // for SocketBuffered
#include "socket_priv.h" // for Socket::SocketPriv

#include <cstddef> // for size_t
#include <memory> // for std::unique_ptr

struct SocketBuffered::SocketBufferedPriv : public Socket::SocketPriv
{
  std::unique_ptr<SocketBufferPool> pool;
  size_t rxBufSize;

  SocketBufferedPriv(Socket::SocketPriv &&sock,
                     size_t rxBufCount,
                     size_t rxBufSize);
  SocketBufferedPriv(SocketBufferedPriv const &) = delete;
  SocketBufferedPriv(SocketBufferedPriv &&other);
  virtual ~SocketBufferedPriv();

  SocketBufferPtr GetBuffer();

  SocketBufferPtr Receive(Socket::Time timeout);
  std::tuple<SocketBufferPtr, SocketAddress> ReceiveFrom(Socket::Time timeout);
};

#endif // SOCKET_BUFFERED_PRIV_H
