#include "socket_address.h"
#include "socket_address_priv.h" // for SocketAddress::SocketAddressPriv

namespace sockpuppet {

SocketAddress::SocketAddress(std::string const &uri)
  : m_priv(std::make_shared<SockAddrInfo>(uri))
{
}

SocketAddress::SocketAddress(std::string const &host,
    std::string const &service)
  : m_priv(std::make_shared<SockAddrInfo>(host, service))
{
}

SocketAddress::SocketAddress(uint16_t port)
  : m_priv(std::make_shared<SockAddrInfo>(port))
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

std::string SocketAddress::Host() const
{
  return Priv().Host();
}

std::string SocketAddress::Service() const
{
  return Priv().Service();
}

uint16_t SocketAddress::Port() const
{
  return Priv().Port();
}

bool SocketAddress::IsV6() const
{
  return Priv().IsV6();
}

std::vector<SocketAddress> SocketAddress::LocalAddresses()
{
  return SocketAddressPriv::LocalAddresses();
}

SocketAddress::SocketAddressPriv const &SocketAddress::Priv() const
{
  if(!m_priv) {
    throw std::logic_error("invalid address");
  }
  return *m_priv.get();
}

bool SocketAddress::operator<(SocketAddress const &other) const
{
  return (Priv() < other.Priv());
}


std::string to_string(SocketAddress const &addr)
{
  return to_string(addr.Priv());
}

} // namespace sockpuppet
