#ifdef _WIN32

#include "address_impl.h"

#include <iphlpapi.h> // for GetAdaptersAddresses

#include <cstring> // for std::memcmp
#include <stdexcept> // for std::runtime_error

namespace sockpuppet {

namespace {

struct AdaptersAddresses : private std::string
{
  AdaptersAddresses()
    : std::string(1024 * 16, '\0')
  {
    for(int i = 0; i < 3; ++i) {
      auto size = static_cast<ULONG>(this->size());
      auto res = ::GetAdaptersAddresses(
        AF_UNSPEC, // Family
        0, // Flags,
        nullptr, // Reserved
        Get(), // AdapterAddresses
        &size); // SizePointer
      if(res == NO_ERROR) {
        return;
      } else if(res == ERROR_BUFFER_OVERFLOW) {
        this->resize(size);
      } else {
        break;
      }
    }
    throw std::runtime_error("failed to get adapters info");
  }

  PIP_ADAPTER_ADDRESSES Get() noexcept
  {
    return reinterpret_cast<PIP_ADAPTER_ADDRESSES>(this->data());
  }
};

bool IsEqualAddr(SockAddrView lhs, SOCKET_ADDRESS const &rhs, int family)
{
  if(lhs.addrLen != rhs.iSockaddrLength)
    return false;

  // explicitly not comparing port number
  if(family == AF_INET6)
    return (0 == memcmp(
      &reinterpret_cast<sockaddr_in6 const *>(lhs.addr)->sin6_addr,
      &reinterpret_cast<sockaddr_in6 const *>(rhs.lpSockaddr)->sin6_addr,
      sizeof(in6_addr)));
  else
    return (0 == memcmp(
      &reinterpret_cast<sockaddr_in const *>(lhs.addr)->sin_addr,
      &reinterpret_cast<sockaddr_in const *>(rhs.lpSockaddr)->sin_addr,
      sizeof(in_addr)));
}

} // unnamed namespace

std::vector<Address>
Address::AddressImpl::LocalAddresses()
{
  // a special host name provides a list of local machine interface addresses
  SockAddrInfo const sockAddr("..localmachine");

  size_t count = 0U;
  for(auto it = sockAddr.info.get(); it != nullptr; it = it->ai_next) {
    ++count;
  }

  std::vector<Address> ret;
  ret.reserve(count);
  for(auto it = sockAddr.info.get(); it != nullptr; it = it->ai_next) {
    ret.emplace_back(std::make_shared<SockAddrStorage>(
                       it->ai_addr,
                       it->ai_addrlen));
  }
  return ret;
}

unsigned int Address::AddressImpl::LocalInterfaceIndex() const
{
  auto const family = Family();
  auto sockAddr = ForAny();

  auto adapters = AdaptersAddresses();
  for(auto adapter = adapters.Get(); adapter; adapter = adapter->Next) {
    for(auto address = adapter->FirstUnicastAddress; address; address = address->Next) {
      if(address->Address.lpSockaddr->sa_family != family)
        continue;

      if(!IsEqualAddr(sockAddr, address->Address, family))
        continue;

      if(family == AF_INET6)
        return adapter->Ipv6IfIndex;
      else
        return adapter->IfIndex;
    }
  }
  throw std::runtime_error("failed to determine local address interface index");
}

} // namespace sockpuppet

#endif // _WIN32
