#ifndef SOCKPUPPET_ADDRESS_IMPL_H
#define SOCKPUPPET_ADDRESS_IMPL_H

#include "sockpuppet/address.h" // for Address
#include "winsock_guard.h" // for WinSockGuard

#ifdef _WIN32
# include <ws2tcpip.h> // for sockaddr_storage
#else
# include <netdb.h> // for sockaddr_storage
# include <sys/socket.h> // for socklen_t
#endif // _WIN32

#include <cstdint> // for uint16_t
#include <memory> // for std::unique_ptr
#include <string> // for std::string
#include <vector> // for std::vector

namespace sockpuppet {

struct SockAddrView
{
  sockaddr const *addr;
  socklen_t addrLen;

  bool operator<(SockAddrView const &other) const;
  bool operator==(SockAddrView const &other) const;
  bool operator!=(SockAddrView const &other) const;
};

struct Address::AddressImpl
{
  WinSockGuard guard;  ///< Guard to initialize socket subsystem on windows

  AddressImpl();
  virtual ~AddressImpl() noexcept;

  virtual SockAddrView ForTcp() const = 0;
  virtual SockAddrView ForUdp() const = 0;
  virtual SockAddrView ForAny() const = 0;
  virtual int Family() const = 0;

  std::string Host() const;
  std::string Service() const;
  uint16_t Port() const;
  bool IsV6() const;

  bool operator<(Address::AddressImpl const &other) const;
  bool operator==(Address::AddressImpl const &other) const;
  bool operator!=(Address::AddressImpl const &other) const;

  static std::vector<Address> LocalAddresses();
};

struct SockAddrInfo : public Address::AddressImpl
{
  struct AddrInfoDeleter
  {
    void operator()(addrinfo const *ptr) const noexcept;
  };
  using AddrInfoPtr = std::unique_ptr<addrinfo const, AddrInfoDeleter>;

  AddrInfoPtr info;

  SockAddrInfo(std::string const &uri);
  SockAddrInfo(std::string const &host, std::string const &serv);
  SockAddrInfo(uint16_t port);
  ~SockAddrInfo() noexcept override;

  addrinfo const *Find(int type, int protocol) const;

  SockAddrView ForTcp() const override;
  SockAddrView ForUdp() const override;
  SockAddrView ForAny() const override;
  int Family() const override;
};

struct SockAddrStorage : public Address::AddressImpl
{
  sockaddr_storage storage;
  socklen_t size;

  SockAddrStorage();
  SockAddrStorage(sockaddr const *addr, size_t addrLen);
  ~SockAddrStorage() noexcept override;

  sockaddr *Addr();
  socklen_t *AddrLen();

  SockAddrView ForTcp() const override;
  SockAddrView ForUdp() const override;
  SockAddrView ForAny() const override;
  int Family() const override;
};

std::string to_string(Address::AddressImpl const &sockAddr);

std::string to_string(SockAddrView const &sockAddr);

} // namespace sockpuppet

namespace std {

template<>
struct hash<sockpuppet::Address::AddressImpl>
{
  size_t operator()(sockpuppet::Address::AddressImpl const &addr) const;
};

} // namespace std

#endif // SOCKPUPPET_ADDRESS_IMPL_H
