#include "sockpuppet/address.h"
#include "address_priv.h" // for Address::AddressPriv

namespace sockpuppet {

Address::Address(std::string const &uri)
  : priv(std::make_shared<SockAddrInfo>(uri))
{
}

Address::Address(std::string const &host,
    std::string const &service)
  : priv(std::make_shared<SockAddrInfo>(host, service))
{
}

Address::Address(uint16_t port)
  : priv(std::make_shared<SockAddrInfo>(port))
{
}

bool Address::operator<(Address const &other) const
{
  return (*priv < *other.priv);
}

bool Address::operator==(Address const &other) const
{
  return (*priv == *other.priv);
}

bool Address::operator!=(Address const &other) const
{
  return (*priv != *other.priv);
}

std::string Address::Host() const
{
  return priv->Host();
}

std::string Address::Service() const
{
  return priv->Service();
}

uint16_t Address::Port() const
{
  return priv->Port();
}

bool Address::IsV6() const
{
  return priv->IsV6();
}

std::vector<Address> Address::LocalAddresses()
{
  return AddressPriv::LocalAddresses();
}

Address::Address(std::shared_ptr<AddressPriv> other)
  : priv(std::move(other))
{
}

Address::Address(Address const &other) = default;

Address::Address(Address &&other) noexcept = default;

Address::~Address() = default;

Address &Address::operator=(Address const &other) = default;

Address &Address::operator=(Address &&other) noexcept = default;


std::string to_string(Address const &addr)
{
  return to_string(*addr.priv);
}

} // namespace sockpuppet
