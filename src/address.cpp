#include "sockpuppet/address.h"
#include "address_priv.h" // for Address::AddressPriv

namespace sockpuppet {

Address::Address(std::string const &uri)
  : m_priv(std::make_shared<SockAddrInfo>(uri))
{
}

Address::Address(std::string const &host,
    std::string const &service)
  : m_priv(std::make_shared<SockAddrInfo>(host, service))
{
}

Address::Address(uint16_t port)
  : m_priv(std::make_shared<SockAddrInfo>(port))
{
}

Address::Address(std::shared_ptr<AddressPriv> other)
  : m_priv(std::move(other))
{
}

Address::Address(Address const &other) = default;

Address::Address(Address &&other) noexcept = default;

Address::~Address() = default;

Address &Address::operator=(Address const &other) = default;

Address &Address::operator=(Address &&other) noexcept = default;

std::string Address::Host() const
{
  return Priv().Host();
}

std::string Address::Service() const
{
  return Priv().Service();
}

uint16_t Address::Port() const
{
  return Priv().Port();
}

bool Address::IsV6() const
{
  return Priv().IsV6();
}

std::vector<Address> Address::LocalAddresses()
{
  return AddressPriv::LocalAddresses();
}

Address::AddressPriv const &Address::Priv() const
{
  if(!m_priv) {
    throw std::logic_error("invalid address");
  }
  return *m_priv.get();
}

bool Address::operator<(Address const &other) const
{
  return (Priv() < other.Priv());
}

bool Address::operator==(Address const &other) const
{
  return (Priv() == other.Priv());
}

bool Address::operator!=(Address const &other) const
{
  return (Priv() != other.Priv());
}


std::string to_string(Address const &addr)
{
  return to_string(addr.Priv());
}

} // namespace sockpuppet
