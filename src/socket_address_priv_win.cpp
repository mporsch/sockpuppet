#ifdef _WIN32

#include "socket_address_priv.h"

namespace sockpuppet {

std::vector<SocketAddress>
SocketAddress::SocketAddressPriv::LocalAddresses()
{
  std::vector<SocketAddress> ret;

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
