#include "socket_address.h"
#include "socket_address_priv.h" // for SocketAddress::SocketAddressPriv

SocketAddress::SocketAddress(std::string const &uri)
  : m_priv(std::make_shared<SocketAddressAddrinfo>(uri))
{
}

SocketAddress::SocketAddress(uint16_t port)
  : m_priv(std::make_shared<SocketAddressAddrinfo>(port))
{
}

SocketAddress::SocketAddress(std::shared_ptr<SocketAddressPriv> other)
  : m_priv(std::move(other))
{
}

SocketAddress::SocketAddress(SocketAddress const &other)
  : m_priv(other.m_priv)
{
}

SocketAddress::SocketAddress(SocketAddress &&other) noexcept
  : m_priv(std::move(other.m_priv))
{
}

SocketAddress::~SocketAddress() = default;

SocketAddress &SocketAddress::operator=(SocketAddress const &other)
{
  m_priv = other.m_priv;
  return *this;
}

SocketAddress &SocketAddress::operator=(SocketAddress &&other) noexcept
{
  m_priv = std::move(other.m_priv);
  return *this;
}

SocketAddress::SocketAddressPriv const *SocketAddress::Priv() const
{
  return m_priv.get();
}


bool operator<(SocketAddress const &lhs, SocketAddress const &rhs)
{
  return (*lhs.Priv() < *rhs.Priv());
}

namespace std {
  std::string to_string(SocketAddress const &addr)
  {
    return (addr.Priv() ?
      std::to_string(addr.Priv()->SockAddrUdp()) :
      "invalid");
  }
} // namespace std
