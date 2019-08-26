#ifndef SOCKET_ADDRESS_PRIV_H
#define SOCKET_ADDRESS_PRIV_H

#include "socket_address.h" // for SocketAddress

#ifdef _WIN32
# pragma push_macro("NOMINMAX")
# undef NOMINMAX
# define NOMINMAX // to avoid overwriting min()/max()
# include <WS2tcpip.h> // for sockaddr_storage
# pragma pop_macro("NOMINMAX")
#else
# include <netdb.h> // for sockaddr_storage
# include <sys/socket.h> // for socklen_t
#endif // _WIN32

#include <cstdint> // for uint16_t
#include <memory> // for std::unique_ptr
#include <string> // for std::string
#include <vector> // for std::vector

namespace sockpuppet {

template <typename T>
struct CDeleter
{
  using DeleterFn = void(*)(T*);

  DeleterFn fn;

  CDeleter(DeleterFn fn)
    : fn(fn)
  {
  }

  void operator()(T *ptr)
  {
    fn(ptr);
  }
};

struct SockAddr
{
  sockaddr const *addr;
  socklen_t addrLen;
  int family;
};

bool operator<(SockAddr const &lhs,
               SockAddr const &rhs);

struct SocketAddress::SocketAddressPriv
{
  virtual ~SocketAddressPriv();
  virtual SockAddr SockAddrTcp() const = 0;
  virtual SockAddr SockAddrUdp() const = 0;
  virtual int Family() const = 0;

  std::string Host() const;
  std::string Service() const;
  bool IsV6() const;

  static std::vector<SocketAddress> GetLocalInterfaceAddresses();
};

bool operator<(SocketAddress::SocketAddressPriv const &lhs,
               SocketAddress::SocketAddressPriv const &rhs);

struct SocketAddressAddrinfo : public SocketAddress::SocketAddressPriv
{
  using AddrInfoPtr = std::unique_ptr<addrinfo, CDeleter<addrinfo>>;

  AddrInfoPtr info;

  SocketAddressAddrinfo(std::string const &uri);
  SocketAddressAddrinfo(std::string const &host, std::string const &serv);
  SocketAddressAddrinfo(uint16_t port);

  addrinfo const *Find(int type, int protocol) const;

  SockAddr SockAddrTcp() const override;
  SockAddr SockAddrUdp() const override;
  int Family() const override;
};

struct SocketAddressStorage : public SocketAddress::SocketAddressPriv
{
  sockaddr_storage storage;
  socklen_t size;

  SocketAddressStorage();

  sockaddr *Addr();
  socklen_t *AddrLen();

  SockAddr SockAddrTcp() const override;
  SockAddr SockAddrUdp() const override;
  int Family() const override;
};

std::string to_string(SockAddr const &sockAddr);

} // namespace sockpuppet

#endif // SOCKET_ADDRESS_PRIV_H
