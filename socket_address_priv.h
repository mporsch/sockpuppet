#ifndef SOCKET_ADDRESS_PRIV_H
#define SOCKET_ADDRESS_PRIV_H

#include "socket_address.h"

#include <netdb.h> // for addrinfo

#include <cstdint> // for uint16_t
#include <memory> // for std::unique_ptr
#include <string> // for std::string

struct AddrInfoDeleter
{
  void operator()(addrinfo *ptr);
};
using AddrInfoPtr = std::unique_ptr<addrinfo, AddrInfoDeleter>;

struct SocketAddress::SocketAddressPriv
{
  AddrInfoPtr info;

  SocketAddressPriv(std::string const &uri);
  SocketAddressPriv(uint16_t port);
};

namespace std {
  std::string to_string(SocketAddress::SocketAddressPriv const &addr);
}

#endif // SOCKET_ADDRESS_PRIV_H
