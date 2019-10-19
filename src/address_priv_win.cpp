#ifdef _WIN32

#include "address_priv.h"

namespace sockpuppet {

std::vector<Address>
Address::AddressPriv::LocalAddresses()
{
  std::vector<Address> ret;

  // a special host name provides a list of local machine interface addresses
  const SockAddrInfo sockAddr("..localmachine");

  for(auto it = sockAddr.info.get(); it != nullptr; it = it->ai_next) {
    ret.emplace_back(std::make_shared<SockAddrStorage>(
                       it->ai_addr,
                       it->ai_addrlen));
  }

  return ret;
}

} // namespace sockpuppet

#endif // _WIN32
