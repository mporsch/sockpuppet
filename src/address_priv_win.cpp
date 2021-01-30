#ifdef _WIN32

#include "address_priv.h"

#include <algorithm> // for std::transform

namespace sockpuppet {

std::vector<Address>
Address::AddressPriv::LocalAddresses()
{
  // a special host name provides a list of local machine interface addresses
  SockAddrInfo const sockAddr("..localmachine");

  auto count = std::distance(
        make_ai_iterator(sockAddr.info.get()),
        make_ai_iterator(nullptr));

  std::vector<Address> ret;
  ret.reserve(count);
  (void)std::transform(
        make_ai_iterator(sockAddr.info.get()),
        make_ai_iterator(nullptr),
        std::back_inserter(ret),
        [](addrinfo const &ai) -> Address {
          return Address(std::make_shared<SockAddrStorage>(
              ai.ai_addr,
              ai.ai_addrlen));
        });
  return ret;
}

} // namespace sockpuppet

#endif // _WIN32
