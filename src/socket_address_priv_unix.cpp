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
    if(auto const res = ::getifaddrs(&addrsRaw)) {
      throw std::runtime_error("failed to get local interface addresses: "
                               + std::string(std::strerror(errno)));
    }
    return make_unique(addrsRaw, ::freeifaddrs);
  }
} // unnamed namespace

SocketAddress SocketAddress::SocketAddressPriv::ToBroadcast(uint16_t port) const
{
  if(IsV6()) {
    throw std::invalid_argument("there are no IPv6 broadcast addresses");
  }

  // get a list of local machine interface addresses
  auto const ifAddrs = GetIfAddrs();

  auto const sockAddr = ForUdp();

  auto isSameHost = [](sockaddr const *lhs, sockaddr const *rhs) -> bool {
    return (reinterpret_cast<sockaddr_in const *>(lhs)->sin_addr.s_addr
            == reinterpret_cast<sockaddr_in const *>(rhs)->sin_addr.s_addr);
  };

  auto setPort = [](sockaddr *out, uint16_t port) {
    reinterpret_cast<sockaddr_in *>(out)->sin_port = htons(port);
  };

  for(auto it = ifAddrs.get(); it != nullptr; it = it->ifa_next) {
    if((it->ifa_addr != nullptr) &&
       (it->ifa_netmask != nullptr) &&
       (it->ifa_addr->sa_family == AF_INET) &&
       ((it->ifa_flags & IFF_LOOPBACK) == 0) &&
       ((it->ifa_flags & IFF_BROADCAST) != 0)) {
      if(isSameHost(it->ifa_addr, sockAddr.addr)) {
        auto sas = std::make_shared<SockAddrStorage>(
                     it->ifa_ifu.ifu_broadaddr,
                     sockAddr.addrLen);
        setPort(sas->Addr(), port);

        return SocketAddress(std::move(sas));
      }
    }
  }

  throw std::runtime_error("failed to get broadcast address matching to \""
                           + to_string(*this) + "\"");
}

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
