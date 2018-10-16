#include "socket_address_priv.h"
#include "socket_guard.h" // for SocketGuard

#ifdef _WIN32
# pragma comment(lib, "Ws2_32.lib")
#endif // _WIN32

#include <algorithm> // for std::count
#include <stdexcept> // for std::runtime_error

namespace {
  bool IsNumeric(char c)
  {
    return (c == '[' ||
            c == ':' ||
            (c >= '0' && c <= '9'));
  }

  bool IsNumeric(std::string const &host)
  {
    return (std::count(std::begin(host), std::end(host), ':') > 1U);
  }

  AddrInfoPtr ParseUri(std::string const &uri)
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
      if(auto const result = getaddrinfo(host.c_str(), serv.c_str(),
                                         &hints, &info)) {
        throw std::runtime_error("failed to parse address \""
                                 + uri + "\": "
                                 + gai_strerror(result));
      }
    }
    return AddrInfoPtr(info);
  }

  AddrInfoPtr ParsePort(std::string const &port)
  {
    SocketGuard guard;

    addrinfo hints{};
    hints.ai_family = AF_INET; // force IPv4 here
    hints.ai_flags = AI_NUMERICSERV | AI_PASSIVE;
    addrinfo *info;
    if(auto const result = getaddrinfo(nullptr, port.c_str(),
                                       &hints, &info)) {
      throw std::runtime_error("failed to parse port "
                               + port + ": "
                               + gai_strerror(result));
    }
    return AddrInfoPtr(info);
  }
} // unnamed namespace

void AddrInfoDeleter::operator()(addrinfo *ptr)
{
  freeaddrinfo(ptr);
}


SocketAddressAddrinfo::SocketAddressAddrinfo(std::string const &uri)
  : info(ParseUri(uri))
{
}

SocketAddressAddrinfo::SocketAddressAddrinfo(uint16_t port)
  : info(ParsePort(std::to_string(port)))
{
}

SockAddr SocketAddressAddrinfo::SockAddrTcp() const
{
  // windows does not explicitly set socktype/protocol, unix does
  for(auto it = info.get(); it != nullptr; it = it->ai_next) {
    if((it->ai_socktype == 0 || it->ai_socktype == SOCK_STREAM) &&
       (it->ai_protocol == 0 || it->ai_protocol == IPPROTO_TCP)) {
      return SockAddr{it->ai_addr, static_cast<socklen_t>(it->ai_addrlen)};
    }
  }
  throw std::logic_error("address is not valid for TCP");
}

SockAddr SocketAddressAddrinfo::SockAddrUdp() const
{
  // windows does not explicitly set socktype/protocol, unix does
  for(auto it = info.get(); it != nullptr; it = it->ai_next) {
    if((it->ai_socktype == 0 || it->ai_socktype == SOCK_DGRAM) &&
       (it->ai_protocol == 0 || it->ai_protocol == IPPROTO_UDP)) {
      return SockAddr{it->ai_addr, static_cast<socklen_t>(it->ai_addrlen)};
    }
  }
  throw std::logic_error("address is not valid for UDP");
}

int SocketAddressAddrinfo::Family() const
{
  auto const first = info.get();
  for(auto it = first->ai_next; it != nullptr; it = it->ai_next) {
    if(first->ai_family != it->ai_family) {
      throw std::logic_error("address contains multiple families");
    }
  }
  return first->ai_family;
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

namespace std {
  std::string to_string(SocketAddress::SocketAddressPriv const &addr)
  {
    SocketGuard guard;

    auto const sockAddr = addr.SockAddrUdp();
    char host[NI_MAXHOST];
    char service[NI_MAXSERV];
    if(auto const result = getnameinfo(
        sockAddr.addr, sockAddr.addrLen,
        host, sizeof(host),
        service, sizeof(service),
        NI_NUMERICHOST | NI_NUMERICSERV)) {
      throw std::runtime_error(std::string("failed to print address: ")
                               + gai_strerror(result));
    }

    return (addr.Family() == AF_INET ?
      std::string(host) + ":" + service :
      std::string("[") + host + "]" + ":" + service);
  }
} // namespace std
