#include "socket_address_priv.h"
#include "socket_guard.h" // for SocketGuard

#ifdef _WIN32
# include <iphlpapi.h>
#else
# include <ifaddrs.h> // for ::getifaddrs
# include <net/if.h> // for IFF_LOOPBACK
#endif // _WIN32

#include <algorithm> // for std::count
#include <cstring> // for std::memcmp
#include <stdexcept> // for std::runtime_error

namespace sockpuppet {

namespace {
  bool IsNumeric(char c)
  {
    return (c == '[' ||
            c == ':' ||
            (c >= '0' && c <= '9'));
  }

  bool IsNumeric(std::string const &host)
  {
    return (std::count(std::begin(host), std::end(host), ':') > 1);
  }

  SocketAddressAddrinfo::AddrInfoPtr ParseUri(std::string const &uri)
  {
    if(uri.empty()) {
      throw std::invalid_argument("empty uri");
    }

    std::string serv;
    std::string host;
    bool isNumericServ = false;
    bool isNumericHost = false;
    {
      auto posServ = uri.find("://");
      if(posServ != std::string::npos) {
        // uri of type serv://host/path
        serv = uri.substr(0U, posServ);

        if(uri.size() > posServ + 3U) {
          host = uri.substr(posServ + 3U);
          isNumericHost = IsNumeric(host[0]) || IsNumeric(host);
        } else {
          // uri of type serv://
        }
      } else {
        posServ = uri.find("]:");
        if(posServ != std::string::npos
        && uri.size() > posServ + 2U) {
          // uri of type [IPv6-host]:serv/path
          serv = uri.substr(posServ + 2U);
          isNumericServ = true;

          host = uri.substr(1U, posServ - 1U);
          isNumericHost = true;
        } else {
          posServ = uri.find_last_of(':');
          if(posServ != std::string::npos
          && uri.size() > posServ + 1U) {
            if(uri.find(':') == posServ) {
              // uri of type IPv4-host:serv/path
              serv = uri.substr(posServ + 1U);
              isNumericServ = true;

              host = uri.substr(0U, posServ);
              isNumericHost = IsNumeric(host[0]);
            } else {
              // uri of type IPv6-host/path
              host = uri;
              isNumericHost = true;
            }
          } else {
            // uri of type host/path
            host = uri;
            isNumericHost = IsNumeric(host[0]) || IsNumeric(host);
          }
        }
      }

      auto const posPath = host.find_last_of('/');
      if(posPath != std::string::npos) {
        host.resize(posPath);
      }
    }

    addrinfo *info;
    {
      SocketGuard guard;

      addrinfo hints{};
      hints.ai_family = AF_UNSPEC;
      hints.ai_flags = AI_PASSIVE |
          (isNumericHost ? AI_NUMERICHOST : 0) |
          (isNumericServ ? AI_NUMERICSERV : 0);
      if(auto const result = ::getaddrinfo(host.c_str(), serv.c_str(),
                                         &hints, &info)) {
        throw std::runtime_error("failed to parse address \""
                                 + uri + "\": "
                                 + ::gai_strerror(result));
      }
    }
    return make_unique(info, ::freeaddrinfo);
  }

  SocketAddressAddrinfo::AddrInfoPtr ParseHostServ(std::string const &host,
      std::string const &serv)
  {
    if(host.empty()) {
      throw std::invalid_argument("empty host");
    } else if(serv.empty()) {
      throw std::invalid_argument("empty service");
    }

    addrinfo *info;
    {
      SocketGuard guard;

      addrinfo hints{};
      hints.ai_family = AF_UNSPEC;
      hints.ai_flags = AI_PASSIVE;
      if(auto const result = ::getaddrinfo(host.c_str(), serv.c_str(),
                                         &hints, &info)) {
        throw std::runtime_error("failed to parse host/port \""
                                 + host + "\", \""
                                 + serv + "\": "
                                 + ::gai_strerror(result));
      }
    }
    return make_unique(info, ::freeaddrinfo);
  }

  SocketAddressAddrinfo::AddrInfoPtr ParsePort(std::string const &port)
  {
    SocketGuard guard;

    addrinfo hints{};
    hints.ai_family = AF_INET; // force IPv4 here
    hints.ai_flags = AI_NUMERICSERV | AI_PASSIVE;
    addrinfo *info;
    if(auto const result = ::getaddrinfo("localhost", port.c_str(),
                                       &hints, &info)) {
      throw std::runtime_error("failed to parse port "
                               + port + ": "
                               + ::gai_strerror(result));
    }
    return make_unique(info, ::freeaddrinfo);
  }
} // unnamed namespace

bool SockAddr::operator<(SockAddr const &other) const
{
  if(addrLen < other.addrLen) {
    return true;
  } else if(addrLen > other.addrLen) {
    return false;
  } else {
    auto const cmp = std::memcmp(addr, other.addr,
      static_cast<size_t>(addrLen));
    return (cmp < 0);
  }
}


SocketAddress::SocketAddressPriv::~SocketAddressPriv() = default;

std::string SocketAddress::SocketAddressPriv::Host() const
{
  SocketGuard guard;

  auto const sockAddr = SockAddrUdp();

  char host[NI_MAXHOST];
  if(auto const result = ::getnameinfo(
      sockAddr.addr, sockAddr.addrLen,
      host, sizeof(host),
      nullptr, 0,
      NI_NUMERICHOST)) {
    throw std::runtime_error(std::string("failed to print host: ")
                             + ::gai_strerror(result));
  }

  return std::string(host);
}

std::string SocketAddress::SocketAddressPriv::Service() const
{
  SocketGuard guard;

  auto const sockAddr = SockAddrUdp();

  char service[NI_MAXSERV];
  if(auto const result = ::getnameinfo(
      sockAddr.addr, sockAddr.addrLen,
      nullptr, 0,
      service, sizeof(service),
      NI_NUMERICSERV)) {
    throw std::runtime_error(std::string("failed to print service: ")
                             + ::gai_strerror(result));
  }

  return std::string(service);
}

bool SocketAddress::SocketAddressPriv::IsV6() const
{
  return (Family() == AF_INET6);
}

SocketAddress SocketAddress::SocketAddressPriv::ToBroadcast() const
{
  if(IsV6()) {
    throw std::invalid_argument("there are no IPv6 broadcast addresses");
  }

  auto const host = Host();

#ifdef _WIN32

  // GetAdaptersInfo returns IPv4 addresses only
  auto getAdaptersInfo = []() -> std::unique_ptr<char const[]> {
    ULONG storageSize = 4096U;
    std::unique_ptr<char[]> storage(new char[storageSize]);

    for(int i = 0; i < 2; ++i) {
      if(::GetAdaptersInfo(
          reinterpret_cast<IP_ADAPTER_INFO *>(storage.get()),
          &storageSize)) {
        // may fail once on insufficient buffer size -> try again with updated size
        storage.reset(new char[storageSize]);
      } else {
        return storage;
      }
    }
    throw std::runtime_error("failed to get addresses using ::GetAdaptersInfo");
  };

  auto fillPort = [](sockaddr *out, sockaddr const *in) {
    reinterpret_cast<sockaddr_in *>(out)->sin_port =
        reinterpret_cast<sockaddr_in const *>(in)->sin_port;
  };

  auto fillBroadcast = [](sockaddr *bcast, sockaddr const *ucast, sockaddr const *mask) {
    // broadcast = (unicast | ~mask) for IPv4
    reinterpret_cast<sockaddr_in *>(bcast)->sin_addr.S_un.S_addr =
        reinterpret_cast<sockaddr_in const *>(ucast)->sin_addr.S_un.S_addr
        | ~reinterpret_cast<sockaddr_in const *>(mask)->sin_addr.S_un.S_addr;
  };

  auto const adaptersInfoStorage = getAdaptersInfo();
  auto const adaptersInfo = reinterpret_cast<IP_ADAPTER_INFO const *>(adaptersInfoStorage.get());

  for(auto adapter = adaptersInfo; adapter != nullptr; adapter = adapter->Next) {
    for(auto address = &adapter->IpAddressList; address != nullptr; address = address->Next) {
      if(address->IpAddress.String == host) {
        const SocketAddressAddrinfo addr(address->IpAddress.String);
        const SocketAddressAddrinfo mask(address->IpMask.String);

        auto ss = std::make_shared<SocketAddressStorage>();
        ss->Addr()->sa_family = AF_INET;
        fillPort(ss->Addr(), SockAddrUdp().addr);
        fillBroadcast(ss->Addr(), addr.SockAddrUdp().addr, mask.SockAddrUdp().addr);
        *ss->AddrLen() = static_cast<socklen_t>(sizeof(sockaddr_in));

        return SocketAddress(std::move(ss));
      }
    }
  }

#else

  ifaddrs *addrsRaw;
  if(auto const res = ::getifaddrs(&addrsRaw)) {
    throw std::runtime_error("failed to get local interface addresses: "
                             + std::string(std::strerror(errno)));
  }
  auto const addrs = make_unique(addrsRaw, ::freeifaddrs);

  for(auto it = addrs.get(); it != nullptr; it = it->ifa_next) {
    if((it->ifa_addr != nullptr) &&
       (it->ifa_netmask != nullptr) &&
       (it->ifa_addr->sa_family == AF_INET) &&
       ((it->ifa_flags & IFF_LOOPBACK) == 0) &&
       ((it->ifa_flags & IFF_BROADCAST) != 0)) {
      auto ss = std::make_shared<SocketAddressStorage>();
      ss->size = static_cast<socklen_t>(sizeof(sockaddr_in));
      std::memcpy(ss->Addr(), it->ifa_addr, ss->size);

      if(ss->Host() == host) {
        std::memcpy(ss->Addr(), it->ifa_ifu.ifu_broadaddr, ss->size);
        return SocketAddress(ss->Host(), Service());
      }
    }
  }

#endif // _WIN32

  throw std::runtime_error("failed to get broadcast address matching to \""
                           + to_string(*this) + "\"");
}

bool SocketAddress::SocketAddressPriv::operator<(SocketAddressPriv const &other) const
{
  return (SockAddrUdp() < other.SockAddrUdp());
}

std::vector<SocketAddress> SocketAddress::SocketAddressPriv::LocalAddresses()
{
  std::vector<SocketAddress> ret;

#ifdef _WIN32
  auto const info = ParseUri("..localmachine");

  for(auto it = info.get(); it != nullptr; it = it->ai_next) {
    auto ss = std::make_shared<SocketAddressStorage>();
    ss->size = static_cast<socklen_t>(it->ai_addrlen);
    std::memcpy(ss->Addr(), it->ai_addr, static_cast<size_t>(ss->size));
    ss->storage.ss_family = static_cast<decltype(ss->storage.ss_family)>(it->ai_family);
    ret.emplace_back(std::move(ss));
  }

#else

  ifaddrs *addrsRaw;
  if(auto const res = ::getifaddrs(&addrsRaw)) {
    throw std::runtime_error("failed to get local interface addresses: "
                             + std::string(std::strerror(errno)));
  }
  auto const addrs = make_unique(addrsRaw, ::freeifaddrs);

  for(auto it = addrs.get(); it != nullptr; it = it->ifa_next) {
    if((it->ifa_addr != nullptr) &&
       (it->ifa_addr->sa_family == AF_INET || it->ifa_addr->sa_family == AF_INET6) &&
       ((it->ifa_flags & IFF_LOOPBACK) == 0)) {
      auto ss = std::make_shared<SocketAddressStorage>();
      ss->size = static_cast<socklen_t>(it->ifa_addr->sa_family == AF_INET ?
                                          sizeof(sockaddr_in) :
                                          sizeof(sockaddr_in6));
      std::memcpy(ss->Addr(), it->ifa_addr, ss->size);
      ret.emplace_back(std::move(ss));
    }
  }
#endif // _WIN32

  return ret;
}


SocketAddressAddrinfo::SocketAddressAddrinfo(std::string const &uri)
  : info(ParseUri(uri))
{
}

SocketAddressAddrinfo::SocketAddressAddrinfo(const std::string &host,
    const std::string &serv)
  : info(ParseHostServ(host, serv))
{
}

SocketAddressAddrinfo::SocketAddressAddrinfo(uint16_t port)
  : info(ParsePort(std::to_string(port)))
{
}

addrinfo const *SocketAddressAddrinfo::Find(int type, int protocol) const
{
  // windows does not explicitly set socktype/protocol, unix does
  for(auto it = info.get(); it != nullptr; it = it->ai_next) {
    if((it->ai_socktype == 0 || it->ai_socktype == type) &&
       (it->ai_protocol == 0 || it->ai_protocol == protocol)) {
      return it;
    }
  }
  return nullptr;
}

SockAddr SocketAddressAddrinfo::SockAddrTcp() const
{
  if(auto const it = Find(SOCK_STREAM, IPPROTO_TCP)) {
    return SockAddr{
      it->ai_addr
    , static_cast<socklen_t>(it->ai_addrlen)
    };
  } else {
    throw std::logic_error("address is not valid for TCP");
  }
}

SockAddr SocketAddressAddrinfo::SockAddrUdp() const
{
  if(auto const it = Find(SOCK_DGRAM, IPPROTO_UDP)) {
    return SockAddr{
      it->ai_addr
    , static_cast<socklen_t>(it->ai_addrlen)
    };
  } else {
    throw std::logic_error("address is not valid for UDP");
  }
}

int SocketAddressAddrinfo::Family() const
{
  // return the family of the first resolved addrinfo
  // in case the provided address was ambiguous,
  // the user can always provide a family-specific string
  return info->ai_family;
}


SocketAddressStorage::SocketAddressStorage()
  : storage{}
  , size(sizeof(storage))
{
}

sockaddr *SocketAddressStorage::Addr()
{
  return reinterpret_cast<sockaddr *>(&storage);
}

socklen_t *SocketAddressStorage::AddrLen()
{
  return &size;
}

SockAddr SocketAddressStorage::SockAddrTcp() const
{
  return SockAddr{
    reinterpret_cast<sockaddr const *>(&storage)
  , size
  };
}

SockAddr SocketAddressStorage::SockAddrUdp() const
{
  return SockAddrTcp();
}

int SocketAddressStorage::Family() const
{
  return storage.ss_family;
}


std::string to_string(SocketAddress::SocketAddressPriv const& sockAddr)
{
  return to_string(sockAddr.SockAddrUdp());
}

std::string to_string(SockAddr const &sockAddr)
{
  SocketGuard guard;

  char host[NI_MAXHOST];
  char service[NI_MAXSERV];
  if(auto const result = ::getnameinfo(
      sockAddr.addr, sockAddr.addrLen,
      host, sizeof(host),
      service, sizeof(service),
      NI_NUMERICHOST | NI_NUMERICSERV)) {
    throw std::runtime_error(std::string("failed to print address: ")
                             + ::gai_strerror(result));
  }

  return (sockAddr.addr->sa_family == AF_INET ?
    std::string(host) + ":" + service :
    std::string("[") + host + "]" + ":" + service);
}

} // namespace sockpuppet
