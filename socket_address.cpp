#include "socket_address.h"
#include "socket_address_priv.h" // for SocketAddress::SocketAddressPriv

SocketAddress::SocketAddress(std::string const &uri)
  : priv(new SocketAddressAddrinfo(uri))
{
}

SocketAddress::SocketAddress(uint16_t port)
  : priv(new SocketAddressAddrinfo(port))
{
}

SocketAddress::SocketAddress(SocketAddress &&other)
  : priv(std::move(other.priv))
{
}

SocketAddress::~SocketAddress()
{
}

SocketAddress &SocketAddress::operator=(SocketAddress &&other)
{
  priv = std::move(other.priv);
  return *this;
}

namespace std {
  std::string to_string(SocketAddress const &addr)
  {
    return std::to_string(*addr.priv);
  }
} // namespace std
