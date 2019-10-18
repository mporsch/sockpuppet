#ifndef _WIN32

#include "socket_address_priv.h"
#include "error_code.h" // for SocketError

#include <ifaddrs.h> // for ::getifaddrs
#include <net/if.h> // for IFF_LOOPBACK

namespace sockpuppet {

namespace {
  std::unique_ptr<ifaddrs, CDeleter<ifaddrs>> GetIfAddrs()
  {
    ifaddrs *addrsRaw;
    if(::getifaddrs(&addrsRaw)) {
      throw std::system_error(SocketError(),
            "failed to get local interface addresses");
    }
    return make_unique(addrsRaw, ::freeifaddrs);
  }
} // unnamed namespace

std::vector<SocketAddress>
SocketAddress::SocketAddressPriv::LocalAddresses()
{
  std::vector<SocketAddress> ret;

  // get a list of local machine interface addresses
  auto const ifAddrs = GetIfAddrs();

  for(auto it = ifAddrs.get(); it != nullptr; it = it->ifa_next) {
    if((it->ifa_addr != nullptr) &&
       (it->ifa_addr->sa_family == AF_INET || it->ifa_addr->sa_family == AF_INET6) &&
       ((it->ifa_flags & IFF_LOOPBACK) == 0)) {
      ret.emplace_back(std::make_shared<SockAddrStorage>(
                         it->ifa_addr,
                         it->ifa_addr->sa_family == AF_INET ?
                           sizeof(sockaddr_in) :
                           sizeof(sockaddr_in6)));
    }
  }

  return ret;
}

} // namespace sockpuppet

#endif // _WIN32
