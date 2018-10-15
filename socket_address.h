#ifndef SOCKET_ADDRESS_H
#define SOCKET_ADDRESS_H

#include <cstdint> // for uint16_t
#include <memory> // for std::shared_ptr
#include <string> // for std::string

struct SocketAddress
{
  struct SocketAddressPriv;

  SocketAddress(std::string const &uri);
  SocketAddress(uint16_t port = 0U);
  SocketAddress(std::shared_ptr<SocketAddressPriv> &&other);
  SocketAddress(SocketAddress const &other);
  SocketAddress(SocketAddress &&other);
  ~SocketAddress();

  SocketAddress &operator=(SocketAddress const &other);
  SocketAddress &operator=(SocketAddress &&other);

  std::shared_ptr<SocketAddressPriv> priv;
};

namespace std {
  std::string to_string(SocketAddress const &addr);
}

#endif // SOCKET_ADDRESS_H
