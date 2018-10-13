#ifndef SOCKET_ADDRESS_PRIV_H
#define SOCKET_ADDRESS_PRIV_H

#include "socket_address.h" // for SocketAddress

#ifdef _WIN32
# pragma push_macro("NOMINMAX")
# define NOMINMAX // to avoid overwriting min()/max()
# include <Winsock2.h> // for addrinfo
# pragma pop_macro("NOMINMAX")
#else
# include <netdb.h> // for addrinfo
# include <sys/socket.h> // for socklen_t
#endif // _WIN32

#include <cstdint> // for uint16_t
#include <memory> // for std::unique_ptr
#include <string> // for std::string

struct SockAddr
{
  sockaddr const *addr;
  socklen_t addrLen;
};

struct SocketAddress::SocketAddressPriv
{
  virtual SockAddr SockAddrTcp() const = 0;
  virtual SockAddr SockAddrUdp() const = 0;
  virtual int Family() const = 0;
};

struct AddrInfoDeleter
{
  void operator()(addrinfo *ptr);
};
using AddrInfoPtr = std::unique_ptr<addrinfo, AddrInfoDeleter>;

struct SocketAddressAddrinfo : public SocketAddress::SocketAddressPriv
{
  AddrInfoPtr info;

  SocketAddressAddrinfo(std::string const &uri);
  SocketAddressAddrinfo(uint16_t port);

  SockAddr SockAddrTcp() const override;
  SockAddr SockAddrUdp() const override;
  int Family() const override;
};

namespace std {
  std::string to_string(SocketAddress::SocketAddressPriv const &addr);
}

#endif // SOCKET_ADDRESS_PRIV_H
