#include "socket_address_priv.h"
#include "socket_guard.h" // for SocketGuard

#ifdef _WIN32
# include <Ws2tcpip.h> // for getaddrinfo
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

  addrinfo *ParseUri(std::string const &uri)
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

      auto posPath = host.find_last_of('/');
      if(posPath != std::string::npos) {
        host.resize(posPath);
      }
    }

    SocketGuard guard;

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_PASSIVE |
        (isNumericHost ? AI_NUMERICHOST : 0) |
        (isNumericServ ? AI_NUMERICSERV : 0);
    addrinfo *info;
    if(auto result = getaddrinfo(host.c_str(), serv.c_str(), &hints, &info)) {
      throw std::runtime_error("failed to parse address \""
                               + uri + "\": "
                               + gai_strerror(result));
    }
    return info;
  }

  addrinfo *ParsePort(uint16_t port)
  {
    SocketGuard guard;

    auto const portStr = std::to_string(port);
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_NUMERICSERV | AI_PASSIVE;
    addrinfo *info;
    if(auto result = getaddrinfo(nullptr, portStr.c_str(), &hints, &info)) {
      throw std::runtime_error("failed to parse port "
                               + std::to_string(port) + ": "
                               + gai_strerror(result));
    }
    return info;
  }
} // unnamed namespace

void AddrInfoDeleter::operator()(addrinfo *ptr)
{
  freeaddrinfo(ptr);
}


SocketAddress::SocketAddressPriv::SocketAddressPriv(std::string const &uri)
  : info(ParseUri(uri))
{
}

SocketAddress::SocketAddressPriv::SocketAddressPriv(uint16_t port)
  : info(ParsePort(port))
{
}

namespace std {
  std::string to_string(SocketAddress::SocketAddressPriv const &addr)
  {
    SocketGuard guard;

    char host[NI_MAXHOST];
    char service[NI_MAXSERV];
    if(auto result = getnameinfo(
        addr.info->ai_addr,
        addr.info->ai_addrlen,
        host, NI_MAXHOST,
        service, NI_MAXSERV,
        NI_NUMERICHOST | NI_NUMERICSERV)) {
      throw std::runtime_error(std::string("failed to print address: ")
                               + gai_strerror(result));
    }

    return (addr.info->ai_family == AF_INET ?
      std::string(host) + ":" + service :
      std::string("[") + host + "]" + ":" + service);
  }
} // namespace std
