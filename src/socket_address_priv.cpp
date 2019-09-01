#include "socket_address_priv.h"
#include "socket_guard.h" // for SocketGuard

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

  SockAddrInfo::AddrInfoPtr ParseUri(std::string const &uri)
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
      if(auto const result = ::getaddrinfo(
           host.c_str(), serv.c_str(),
           &hints, &info)) {
        throw std::runtime_error("failed to parse address \""
                                 + uri + "\": "
                                 + ::gai_strerror(result));
      }
    }
    return make_unique(info, ::freeaddrinfo);
  }

  SockAddrInfo::AddrInfoPtr ParseHostServ(std::string const &host,
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
      if(auto const result = ::getaddrinfo(
           host.c_str(), serv.c_str(),
           &hints, &info)) {
        throw std::runtime_error("failed to parse host/port \""
                                 + host + "\", \""
                                 + serv + "\": "
                                 + ::gai_strerror(result));
      }
    }
    return make_unique(info, ::freeaddrinfo);
  }

  SockAddrInfo::AddrInfoPtr ParsePort(std::string const &port)
  {
    SocketGuard guard;

    addrinfo hints{};
    hints.ai_family = AF_INET; // force IPv4 here
    hints.ai_flags = AI_NUMERICSERV | AI_PASSIVE;
    addrinfo *info;
    if(auto const result = ::getaddrinfo(
         "localhost", port.c_str(),
         &hints, &info)) {
      throw std::runtime_error("failed to parse port "
                               + port + ": "
                               + ::gai_strerror(result));
    }
    return make_unique(info, ::freeaddrinfo);
  }
} // unnamed namespace

bool SockAddrView::operator<(SockAddrView const &other) const
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

  auto const sockAddr = ForUdp();

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

  auto const sockAddr = ForUdp();

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

uint16_t SocketAddress::SocketAddressPriv::Port() const
{
  auto const sockAddr = ForUdp();

  return ntohs(IsV6() ?
      reinterpret_cast<sockaddr_in6 const *>(sockAddr.addr)->sin6_port :
      reinterpret_cast<sockaddr_in const *>(sockAddr.addr)->sin_port);
}

bool SocketAddress::SocketAddressPriv::IsV6() const
{
  return (Family() == AF_INET6);
}

bool SocketAddress::SocketAddressPriv::operator<(
    SocketAddressPriv const &other) const
{
  return (ForUdp() < other.ForUdp());
}


SockAddrInfo::SockAddrInfo(std::string const &uri)
  : info(ParseUri(uri))
{
}

SockAddrInfo::SockAddrInfo(const std::string &host,
    const std::string &serv)
  : info(ParseHostServ(host, serv))
{
}

SockAddrInfo::SockAddrInfo(uint16_t port)
  : info(ParsePort(std::to_string(port)))
{
}

addrinfo const *SockAddrInfo::Find(int type, int protocol) const
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

SockAddrView SockAddrInfo::ForTcp() const
{
  if(auto const it = Find(SOCK_STREAM, IPPROTO_TCP)) {
    return SockAddrView{
      it->ai_addr
    , static_cast<socklen_t>(it->ai_addrlen)
    };
  } else {
    throw std::logic_error("address is not valid for TCP");
  }
}

SockAddrView SockAddrInfo::ForUdp() const
{
  if(auto const it = Find(SOCK_DGRAM, IPPROTO_UDP)) {
    return SockAddrView{
      it->ai_addr
    , static_cast<socklen_t>(it->ai_addrlen)
    };
  } else {
    throw std::logic_error("address is not valid for UDP");
  }
}

int SockAddrInfo::Family() const
{
  // return the family of the first resolved addrinfo
  // in case the provided address was ambiguous,
  // the user can always provide a family-specific string
  return info->ai_family;
}


SockAddrStorage::SockAddrStorage()
  : storage{}
  , size(sizeof(storage))
{
}

SockAddrStorage::SockAddrStorage(sockaddr const *addr, size_t addrLen)
  : storage{}
  , size(static_cast<socklen_t>(addrLen))
{
  std::memcpy(&storage, addr, size);
}

sockaddr *SockAddrStorage::Addr()
{
  return reinterpret_cast<sockaddr *>(&storage);
}

socklen_t *SockAddrStorage::AddrLen()
{
  return &size;
}

SockAddrView SockAddrStorage::ForTcp() const
{
  return SockAddrView{
    reinterpret_cast<sockaddr const *>(&storage)
  , size
  };
}

SockAddrView SockAddrStorage::ForUdp() const
{
  return ForTcp();
}

int SockAddrStorage::Family() const
{
  return storage.ss_family;
}


std::string to_string(SocketAddress::SocketAddressPriv const& sockAddr)
{
  return to_string(sockAddr.ForUdp());
}

std::string to_string(SockAddrView const &sockAddr)
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
