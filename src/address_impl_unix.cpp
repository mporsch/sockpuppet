#ifndef _WIN32

#include "address_impl.h"
#include "error_code.h" // for SocketError

#include <ifaddrs.h> // for ::getifaddrs
#include <net/if.h> // for IFF_LOOPBACK

#include <cstring> // for std::memcmp

namespace sockpuppet {

namespace {

struct IfAddrsDeleter
{
  void operator()(ifaddrs const *ptr) const noexcept
  {
    ::freeifaddrs(const_cast<ifaddrs *>(ptr));
  }
};
using IfAddrsPtr = std::unique_ptr<ifaddrs const, IfAddrsDeleter>;

IfAddrsPtr GetIfAddrs()
{
  ifaddrs *addrs;
  if(::getifaddrs(&addrs)) {
    throw std::system_error(SocketError(),
          "failed to get local interface addresses");
  }
  return IfAddrsPtr(addrs);
}

template<typename T>
bool IsEqualAddr(T const &lhs, T const &rhs)
{
  return (0 == std::memcmp(&lhs, &rhs, sizeof(T)));
}

bool IsEqualAddr(SockAddrView lhs, sockaddr const *rhs, int family)
{
  // explicitly not comparing port number
  if(family == AF_INET6)
    return IsEqualAddr(
      reinterpret_cast<sockaddr_in6 const *>(lhs.addr)->sin6_addr,
      reinterpret_cast<sockaddr_in6 const *>(rhs)->sin6_addr);
  else
    return IsEqualAddr(
      reinterpret_cast<sockaddr_in const *>(lhs.addr)->sin_addr,
      reinterpret_cast<sockaddr_in const *>(rhs)->sin_addr);
}

unsigned int NameToIndex(const char *name)
{
  unsigned int idx = 0;
  auto ifIdx = if_nameindex();
  if(ifIdx == nullptr)
    throw std::system_error(SocketError(),
          "failed to get local interface name index");

  for (auto it = ifIdx; !(it->if_index == 0 && it->if_name == nullptr); ++it) {
    if(0 == std::strcmp(it->if_name, name)) {
      idx = it->if_index;
    }
  }

  if_freenameindex(ifIdx);
  return idx;
}

} // unnamed namespace

std::vector<Address>
Address::AddressImpl::LocalAddresses()
{
  // get a list of local machine interface addresses
  auto const ifAddrs = GetIfAddrs();

  size_t count = 0U;
  for(auto it = ifAddrs.get(); it != nullptr; it = it->ifa_next) {
    ++count;
  }

  std::vector<Address> ret;
  ret.reserve(count);
  for(auto it = ifAddrs.get(); it != nullptr; it = it->ifa_next) {
    if((it->ifa_addr != nullptr) &&
       ((it->ifa_flags & IFF_LOOPBACK) == 0)) {
      if(it->ifa_addr->sa_family == AF_INET) {
        ret.emplace_back(std::make_shared<SockAddrStorage>(
              it->ifa_addr, sizeof(sockaddr_in)));
      } else if(it->ifa_addr->sa_family == AF_INET6) {
        ret.emplace_back(std::make_shared<SockAddrStorage>(
              it->ifa_addr, sizeof(sockaddr_in6)));
      }
    }
  }
  return ret;
}

unsigned long Address::AddressImpl::LocalInterfaceIndex() const
{
  auto const family = Family();
  auto const sockAddr = ForAny();

  auto const ifAddrs = GetIfAddrs();
  for(auto it = ifAddrs.get(); it != nullptr; it = it->ifa_next) {
    if(it->ifa_addr == nullptr)
      continue;

    if(it->ifa_addr->sa_family != family)
      continue;

    if(IsEqualAddr(sockAddr, it->ifa_addr, family))
      return NameToIndex(it->ifa_name);
  }
  return 0;
}

} // namespace sockpuppet

#endif // _WIN32
