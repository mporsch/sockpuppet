#include "socket_address.h"
#include "socket_address_priv.h" // for SocketAddress::SocketAddressPriv

SocketAddress::SocketAddress(std::string const &uri)
  : priv(std::make_shared<SocketAddressAddrinfo>(uri))
{
}

SocketAddress::SocketAddress(uint16_t port)
  : priv(std::make_shared<SocketAddressAddrinfo>(port))
{
}

SocketAddress::SocketAddress(std::shared_ptr<SocketAddressPriv> &&other)
  : priv(std::move(other))
{
}

SocketAddress::SocketAddress(SocketAddress const &other)
  : priv(other.priv)
{
}

SocketAddress::SocketAddress(SocketAddress &&other)
  : priv(std::move(other.priv))
{
}

SocketAddress::~SocketAddress()
{
}

SocketAddress &SocketAddress::operator=(SocketAddress const &other)
{
  priv = other.priv;
  return *this;
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