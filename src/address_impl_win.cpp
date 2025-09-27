#ifdef _WIN32

#include "address_impl.h"

#include <iphlpapi.h> // for GetAdaptersAddresses

#include <stdexcept> // for std::runtime_error

namespace sockpuppet {

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
  std::string buffer(1024, '\0');
  for(int i = 0; i < 3; ++i) {
    auto size = static_cast<ULONG>(buffer.size());
    auto res = ::GetAdaptersAddresses(
      AF_UNSPEC, // Family
      0, // Flags,
      nullptr, // Reserved
      reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data()), // AdapterAddresses
      &size); // SizePointer
    if(res == NO_ERROR) {
      break;
    } else if(res == ERROR_BUFFER_OVERFLOW) {
      buffer.resize(size);
    } else {
      throw std::runtime_error("failed to get adapters info");
    }
  }

  //auto const family = Family();
  auto const thisHost = Host();
  for(auto adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES const *>(buffer.data()); adapter; adapter = adapter->Next) {
    for(auto address = adapter->FirstUnicastAddress; address; address = address->Next) {
      auto otherHost = SockAddrStorage(
        reinterpret_cast<sockaddr const *>(address->Address.lpSockaddr),
        static_cast<size_t>(address->Address.iSockaddrLength)
      ).Host();

      if(true &&
          (address->Address.lpSockaddr->sa_family == AF_INET) &&
          (otherHost == thisHost))
        return adapter->IfIndex;
      else if(true &&
          (address->Address.lpSockaddr->sa_family == AF_INET6) &&
          (otherHost == thisHost))
        return adapter->Ipv6IfIndex;
    }
  }
  throw std::runtime_error("failed to determine local address interface index");
}

} // namespace sockpuppet

#endif // _WIN32
