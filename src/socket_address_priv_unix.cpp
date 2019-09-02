#ifndef _WIN32

#include "socket_address_priv.h"

#include <ifaddrs.h> // for ::getifaddrs
#include <net/if.h> // for IFF_LOOPBACK

#include <cstring> // for std::strerror
#include <stdexcept> // for std::runtime_error

namespace sockpuppet {

namespace  {
  std::unique_ptr<ifaddrs, CDeleter<ifaddrs>> GetIfAddrs()
  {
    ifaddrs *addrsRaw;
    if(::getifaddrs(&addrsRaw)) {
      throw std::runtime_error("failed to get local interface addresses: "
                               + std::string(std::strerror(errno)));
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
