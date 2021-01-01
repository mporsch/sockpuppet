#ifdef _WIN32

#include "address_priv.h"

namespace sockpuppet {

std::vector<Address>
Address::AddressPriv::LocalAddresses()
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

} // namespace sockpuppet

#endif // _WIN32
