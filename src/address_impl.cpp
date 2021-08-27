#include "address_impl.h"
#include "error_code.h" // for AddressError

#include <cstring> // for std::memcmp
#include <regex> // for std::regex
#include <string_view> // for std::string_view

namespace sockpuppet {

namespace {

struct UriDissect
{
  std::string serv;
  std::string host;
  addrinfo hints = {};

  UriDissect(std::string_view uri)
  {
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE;

    std::cmatch match;
    static std::regex const reServ(R"(((^\w+)?://)?([^/]+)/?.*$)");
    if(std::regex_match(std::begin(uri), std::end(uri), match, reServ)) {
      if(match[1].matched) {
        // URI of type serv://host/path
        serv = match[2].str();
      }

      // trim serv + path
      uri = {match[3].first, static_cast<size_t>(match[3].length())};

      static std::regex const rePort(R"(((^\[?(.*:\w*:[^\]]*)\]?)|(^[^:]*))(:(\d+$))?)");
      if(std::regex_match(std::begin(uri), std::end(uri), match, rePort)) {
        if(match[6].matched) {
          // URI with numeric port suffix
          serv = match[6].str();
          hints.ai_flags |= AI_NUMERICSERV;
        }
        if(match[3].matched) {
          // URI with IPv6-host
          host = match[3].str();
          hints.ai_flags |= AI_NUMERICHOST;
          return;
        } else if(match[4].matched) {
          host = match[4].str();
          return;
        }
      }
    }
    throw std::logic_error("unexpected regex non-match");
  }
};

SockAddrInfo::AddrInfoPtr ParseUri(std::string const &uri)
{
  if(uri.empty()) {
    throw std::invalid_argument("empty uri");
  }

  addrinfo *info;
  {
    auto dissect = UriDissect(uri);
    if(auto const result = ::getaddrinfo(
         dissect.host.c_str(), dissect.serv.c_str(),
         &dissect.hints, &info)) {
      throw std::system_error(AddressError(result),
            "failed to parse address \"" + uri + "\"");
    }
  }
  return {info, ::freeaddrinfo};
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
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE;
    if(auto const result = ::getaddrinfo(
         host.c_str(), serv.c_str(),
         &hints, &info)) {
      throw std::system_error(AddressError(result),
            "failed to parse host/port \"" + host + "\", \"" + serv + "\"");
    }
  }
  return {info, ::freeaddrinfo};
}

SockAddrInfo::AddrInfoPtr ParsePort(std::string const &port)
{
  addrinfo hints{};
  hints.ai_family = AF_INET; // force IPv4 here
  hints.ai_flags = AI_NUMERICSERV | AI_PASSIVE;
  addrinfo *info;
  if(auto const result = ::getaddrinfo(
       "localhost", port.c_str(),
       &hints, &info)) {
    throw std::system_error(AddressError(result),
          "failed to parse port \"" + port + "\"");
  }
  return {info, ::freeaddrinfo};
}

} // unnamed namespace

bool SockAddrView::operator<(SockAddrView const &other) const
{
  if(addrLen < other.addrLen) {
    return true;
  } else if(addrLen > other.addrLen) {
    return false;
  }
  return (0 > std::memcmp(addr, other.addr, static_cast<size_t>(addrLen)));
}

bool SockAddrView::operator==(SockAddrView const &other) const
{
  return ((addrLen == other.addrLen) &&
          (0 == std::memcmp(addr, other.addr, static_cast<size_t>(addrLen))));
}

bool SockAddrView::operator!=(SockAddrView const &other) const
{
  return !(*this == other);
}


Address::AddressImpl::AddressImpl() = default;

Address::AddressImpl::~AddressImpl() = default;

std::string Address::AddressImpl::Host() const
{
  auto const sockAddr = ForAny();

  std::string host(NI_MAXHOST, '\0');
  if(auto const result = ::getnameinfo(
      sockAddr.addr, sockAddr.addrLen,
      const_cast<char *>(host.data()), NI_MAXHOST,
      nullptr, 0,
      NI_NUMERICHOST)) {
    throw std::system_error(AddressError(result), "failed to print host");
  }
  host.erase(host.find('\0'));

  return host;
}

std::string Address::AddressImpl::Service() const
{
  auto const sockAddr = ForAny();

  std::string service(NI_MAXSERV, '\0');
  if(auto const result = ::getnameinfo(
      sockAddr.addr, sockAddr.addrLen,
      nullptr, 0,
      const_cast<char *>(service.data()), NI_MAXSERV,
      NI_NUMERICSERV)) {
    throw std::system_error(AddressError(result), "failed to print service");
  }
  service.erase(service.find('\0'));

  return service;
}

uint16_t Address::AddressImpl::Port() const
{
  auto const sockAddr = ForAny();
  auto const num = IsV6() ?
        reinterpret_cast<sockaddr_in6 const *>(sockAddr.addr)->sin6_port :
        reinterpret_cast<sockaddr_in const *>(sockAddr.addr)->sin_port;
  return ntohs(num); // careful; ntohs is a fragile macro in OSX
}

bool Address::AddressImpl::IsV6() const
{
  return (Family() == AF_INET6);
}

bool Address::AddressImpl::operator<(
    AddressImpl const &other) const
{
  return (ForAny() < other.ForAny());
}

bool Address::AddressImpl::operator==(
    AddressImpl const &other) const
{
  return (ForAny() == other.ForAny());
}

bool Address::AddressImpl::operator!=(
    AddressImpl const &other) const
{
  return (ForAny() != other.ForAny());
}


SockAddrInfo::SockAddrInfo(std::string const &uri)
  : AddressImpl()
  , info(ParseUri(uri))
{
}

SockAddrInfo::SockAddrInfo(std::string const &host,
    std::string const &serv)
  : AddressImpl()
  , info(ParseHostServ(host, serv))
{
}

SockAddrInfo::SockAddrInfo(uint16_t port)
  : AddressImpl()
  , info(ParsePort(std::to_string(port)))
{
}

SockAddrInfo::~SockAddrInfo() = default;

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
  }
  throw std::logic_error("address is not valid for TCP");
}

SockAddrView SockAddrInfo::ForUdp() const
{
  if(auto const it = Find(SOCK_DGRAM, IPPROTO_UDP)) {
    return SockAddrView{
      it->ai_addr
    , static_cast<socklen_t>(it->ai_addrlen)
    };
  }
  throw std::logic_error("address is not valid for UDP");
}

SockAddrView SockAddrInfo::ForAny() const
{
  return SockAddrView{
    info->ai_addr
  , static_cast<socklen_t>(info->ai_addrlen)
  };
}

int SockAddrInfo::Family() const
{
  // return the family of the first resolved addrinfo
  // in case the provided address was ambiguous,
  // the user can always provide a family-specific string
  return info->ai_family;
}


SockAddrStorage::SockAddrStorage()
  : AddressImpl()
  , storage{}
  , size(sizeof(storage))
{
}

SockAddrStorage::SockAddrStorage(sockaddr const *addr, size_t addrLen)
  : AddressImpl()
  , storage{}
  , size(static_cast<socklen_t>(addrLen))
{
  std::memcpy(&storage, addr, addrLen);
}

SockAddrStorage::~SockAddrStorage() = default;

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
  return ForAny();
}

SockAddrView SockAddrStorage::ForUdp() const
{
  return ForAny();
}

SockAddrView SockAddrStorage::ForAny() const
{
  return SockAddrView{
    reinterpret_cast<sockaddr const *>(&storage)
  , size
  };
}

int SockAddrStorage::Family() const
{
  return storage.ss_family;
}


std::string to_string(Address::AddressImpl const &sockAddr)
{
  return to_string(sockAddr.ForAny());
}

std::string to_string(SockAddrView const &sockAddr)
{
  // buffer for format [host]:serv
  std::string str(NI_MAXHOST + NI_MAXSERV + 3, '\0');
  size_t host = 0;
  size_t serv = NI_MAXHOST + 1;
  bool const isV6 = (sockAddr.addr->sa_family != AF_INET);
  if(isV6) {
    str[host++] = '[';
    serv += 2;
  }

  if(auto const result = ::getnameinfo(
      sockAddr.addr, sockAddr.addrLen,
      &str[host], NI_MAXHOST,
      &str[serv], NI_MAXSERV,
      NI_NUMERICHOST | NI_NUMERICSERV)) {
    throw std::system_error(AddressError(result), "failed to print address");
  }

  str.erase(str.find('\0', serv));
  str[--serv] = ':';
  if(isV6) {
    str[--serv] = ']';
  }
  host = str.find('\0', host);
  str.erase(host, serv - host);

  return str;
}

} // namespace sockpuppet

namespace std {

size_t hash<sockpuppet::Address::AddressImpl>::operator()(
    sockpuppet::Address::AddressImpl const &addr) const
{
  auto sockAddr = addr.ForAny();

  // bytewise hash using string_view
  return hash<string_view>()(string_view(
      reinterpret_cast<char const *>(sockAddr.addr),
      static_cast<size_t>(sockAddr.addrLen)));
}

} // namespace std
