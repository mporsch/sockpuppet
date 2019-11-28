#include "sockpuppet/socket_buffered.h"
#include "socket_buffered_priv.h" // for SocketBufferedPriv

#include <algorithm> // for std::find_if
#include <cassert> // for assert
#include <stdexcept> // for std::runtime_error

namespace sockpuppet {

void BufferPool::Recycler::operator()(Buffer *buf)
{
  assert(buf);

  pool.get().Recycle(buf);
}


BufferPool::BufferPool(size_t maxSize)
  : m_maxSize(maxSize - 1U)
{
  // with given limit, pre-allocate the buffers now
  for(size_t i = 0U; i < maxSize; ++i) {
    m_idle.emplace(std::make_unique<Buffer>());
  }
}

BufferPool::~BufferPool()
{
  assert(m_busy.empty()); // buffers still pending -> will segfault later
}

BufferPool::BufferPtr BufferPool::Get()
{
  std::lock_guard<std::mutex> lock(m_mtx);

  if(m_idle.empty()) {
    if(m_busy.size() <= m_maxSize) {
      // allocate a new buffer already in the busy list
      m_busy.emplace_front(
        std::make_unique<Buffer>());

      // bind to recycler and return
      return {m_busy.front().get(), Recycler{*this}};
    } else {
      throw std::runtime_error("out of buffers");
    }
  } else {
    // move from idle to busy
    m_busy.emplace_front(std::move(m_idle.top()));
    m_idle.pop();

    // clear previous content
    auto &&buf = m_busy.front();
    buf->clear();

    // bind to recycler and return
    return {buf.get(), Recycler{*this}};
  }
}

void BufferPool::Recycle(Buffer *buf)
{
  std::lock_guard<std::mutex> lock(m_mtx);

  auto const it = std::find_if(std::begin(m_busy), std::end(m_busy),
    [&](BufferStorage const &b) -> bool {
      return (b.get() == buf);
    });
  if(it == std::end(m_busy)) {
    throw std::logic_error("returned invalid buffer");
  }

  // move from busy to idle
  m_idle.push(std::move(*it));
  m_busy.erase(it);
}


Address SocketBuffered::LocalAddress() const
{
  return Address(m_priv->GetSockName());
}

size_t SocketBuffered::ReceiveBufferSize() const
{
  return m_priv->GetSockOptRcvBuf();
}

SocketBuffered::SocketBuffered(Socket &&sock,
    size_t rxBufCount, size_t rxBufSize)
  : m_priv(std::make_unique<SocketBufferedPriv>(
      std::move(*sock.m_priv), rxBufCount, rxBufSize))
{
}

SocketBuffered::SocketBuffered(SocketBuffered &&other) noexcept = default;

SocketBuffered::~SocketBuffered() = default;

SocketBuffered &SocketBuffered::operator=(SocketBuffered &&other) noexcept = default;


SocketUdpBuffered::SocketUdpBuffered(SocketUdp &&sock,
    size_t rxBufCount, size_t rxBufSize)
  : SocketBuffered(std::move(sock), rxBufCount, rxBufSize)
{
}

SocketUdpBuffered::SocketUdpBuffered(SocketUdpBuffered &&other) noexcept = default;

SocketUdpBuffered::~SocketUdpBuffered() = default;

SocketUdpBuffered &SocketUdpBuffered::operator=(SocketUdpBuffered &&other) noexcept = default;

size_t SocketUdpBuffered::SendTo(char const *data, size_t size,
    Address const &dstAddress, Duration timeout)
{
  return m_priv->SendTo(data, size, dstAddress.Priv().ForUdp(), timeout);
}

BufferPtr SocketUdpBuffered::Receive(Duration timeout)
{
  return m_priv->Receive(timeout);
}

std::pair<BufferPtr, Address>
SocketUdpBuffered::ReceiveFrom(Duration timeout)
{
  return m_priv->ReceiveFrom(timeout);
}


SocketTcpBuffered::SocketTcpBuffered(SocketTcpClient &&sock,
    size_t rxBufCount, size_t rxBufSize)
  : SocketBuffered(std::move(sock), rxBufCount, rxBufSize)
{
}

SocketTcpBuffered::SocketTcpBuffered(SocketTcpBuffered &&other) noexcept = default;

SocketTcpBuffered::~SocketTcpBuffered() = default;

SocketTcpBuffered &SocketTcpBuffered::operator=(SocketTcpBuffered &&other) noexcept = default;

size_t SocketTcpBuffered::Send(char const *data, size_t size,
    Duration timeout)
{
  return m_priv->Send(data, size, timeout);
}

BufferPtr SocketTcpBuffered::Receive(Duration timeout)
{
  return m_priv->Receive(timeout);
}

Address SocketTcpBuffered::PeerAddress() const
{
  return Address(m_priv->GetPeerName());
}

} // namespace sockpuppet
