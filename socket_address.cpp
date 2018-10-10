#include "socket_address.h"
#include "socket_address_priv.h" // for SocketAddress::SocketAddressPriv

SocketAddress::SocketAddress(std::string const &uri)
  : priv(new SocketAddressPriv(uri))
{
}

SocketAddress::SocketAddress(uint16_t port)
  : priv(new SocketAddressPriv(port))
{
}

SocketAddress::~SocketAddress()
{
}

namespace std {
  std::string to_string(SocketAddress const &addr)
  {
    return std::to_string(*addr.priv);
  }
} // namespace std
