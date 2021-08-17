#include "sockpuppet/socket_buffered.h"
#include "address_impl.h" // for Address::AddressImpl
#include "socket_buffered_impl.h" // for SocketBufferedImpl

#include <algorithm> // for std::find_if
#include <cassert> // for assert
#include <stdexcept> // for std::runtime_error

namespace sockpuppet {

void BufferPool::Recycler::operator()(Buffer *buf)
{
  assert(pool);
  assert(buf);
  pool->Recycle(buf);
}


BufferPool::BufferPool(size_t maxSize)
  : m_maxSize(maxSize - 1U)
{
  // with given limit, pre-allocate the buffers now
  for(size_t i = 0U; i < maxSize; ++i) {
    m_idle.emplace(std::make_unique<Buffer>());
  }
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
      return {m_busy.front().get(), Recycler{this}};
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
    return {buf.get(), Recycler{this}};
  }
}

BufferPool::~BufferPool()
{
  // buffers still pending -> will segfault later
  // make sure pool is released after all of its users
  assert(m_busy.empty());
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


SocketUdpBuffered::SocketUdpBuffered(SocketUdp &&sock,
    size_t rxBufCount, size_t rxBufSize)
  : impl(std::make_unique<SocketBufferedImpl>(
      std::move(sock.impl),
      rxBufCount,
      rxBufSize))
{
}

size_t SocketUdpBuffered::SendTo(char const *data, size_t size,
    Address const &dstAddress, Duration timeout)
{
  return impl->sock->SendTo(data, size, dstAddress.impl->ForUdp(), timeout);
}

std::optional<std::pair<BufferPtr, Address>>
SocketUdpBuffered::ReceiveFrom(Duration timeout)
{
  return impl->ReceiveFrom(timeout);
}

Address SocketUdpBuffered::LocalAddress() const
{
  return Address(impl->sock->GetSockName());
}

SocketUdpBuffered::SocketUdpBuffered(SocketUdpBuffered &&other) noexcept = default;

SocketUdpBuffered::~SocketUdpBuffered() = default;

SocketUdpBuffered &SocketUdpBuffered::operator=(SocketUdpBuffered &&other) noexcept = default;


SocketTcpBuffered::SocketTcpBuffered(SocketTcp &&sock,
    size_t rxBufCount, size_t rxBufSize)
  : impl(std::make_unique<SocketBufferedImpl>(
      std::move(sock.impl),
      rxBufCount,
      rxBufSize))
{
}

size_t SocketTcpBuffered::Send(char const *data, size_t size,
    Duration timeout)
{
  return impl->sock->Send(data, size, timeout);
}

std::optional<BufferPtr> SocketTcpBuffered::Receive(Duration timeout)
{
  return impl->Receive(timeout);
}

Address SocketTcpBuffered::LocalAddress() const
{
  return Address(impl->sock->GetSockName());
}

Address SocketTcpBuffered::PeerAddress() const
{
  return Address(impl->sock->GetPeerName());
}

SocketTcpBuffered::SocketTcpBuffered(SocketTcpBuffered &&other) noexcept = default;

SocketTcpBuffered::~SocketTcpBuffered() = default;

SocketTcpBuffered &SocketTcpBuffered::operator=(SocketTcpBuffered &&other) noexcept = default;

} // namespace sockpuppet
