#include "sockpuppet/address.h"
#include "address_impl.h" // for Address::AddressImpl

namespace sockpuppet {

Address::Address(std::string const &uri)
  : impl(std::make_shared<SockAddrInfo>(uri))
{
}

Address::Address(std::string const &host,
    std::string const &service)
  : impl(std::make_shared<SockAddrInfo>(host, service))
{
}

Address::Address(uint16_t port)
  : impl(std::make_shared<SockAddrInfo>(port))
{
}

bool Address::operator<(Address const &other) const
{
  return (*impl < *other.impl);
}

bool Address::operator==(Address const &other) const
{
  return (*impl == *other.impl);
}

bool Address::operator!=(Address const &other) const
{
  return (*impl != *other.impl);
}

std::string Address::Host() const
{
  return impl->Host();
}

std::string Address::Service() const
{
  return impl->Service();
}

uint16_t Address::Port() const
{
  return impl->Port();
}

bool Address::IsV6() const
{
  return impl->IsV6();
}

std::vector<Address> Address::LocalAddresses()
{
  return AddressImpl::LocalAddresses();
}

Address::Address(std::shared_ptr<AddressImpl> other)
  : impl(std::move(other))
{
}

Address::Address(Address const &other) = default;

Address::Address(Address &&other) noexcept = default;

Address::~Address() = default;

Address &Address::operator=(Address const &other) = default;

Address &Address::operator=(Address &&other) noexcept = default;


std::string to_string(Address const &addr)
{
  return to_string(*addr.impl);
}

} // namespace sockpuppet

namespace std {

size_t hash<sockpuppet::Address>::operator()(sockpuppet::Address const &addr) const
{
  if(!addr.impl) {
    return hash<sockpuppet::Address::AddressImpl *>()(nullptr);
  }
  return hash<sockpuppet::Address::AddressImpl>()(*addr.impl);
}

} // namespace std
