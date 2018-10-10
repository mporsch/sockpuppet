#ifndef SOCKET_ADDRESS_H
#define SOCKET_ADDRESS_H

#include <cstdint> // for uint16_t
#include <memory> // for std::unique_ptr
#include <string> // for std::string

struct SocketAddress
{
  SocketAddress(std::string const &uri);
  SocketAddress(uint16_t port = 0U);
  SocketAddress(SocketAddress &&other);
  ~SocketAddress();

  SocketAddress &operator=(SocketAddress &&other);

  struct SocketAddressPriv;
  std::unique_ptr<SocketAddressPriv> priv;
};

namespace std {
  std::string to_string(SocketAddress const &addr);
}

#endif // SOCKET_ADDRESS_H
