#ifndef SOCKET_ADDRESS_H
#define SOCKET_ADDRESS_H

#include <cstdint> // for uint16_t
#include <memory> // for std::shared_ptr
#include <string> // for std::string
#include <vector> // for std::vector

namespace sockpuppet {

struct SocketAddress
{
  /// Pimpl to hide away the OS-specifics.
  struct SocketAddressPriv;

  /// Create a local/remote host address from given URI.
  /// @param  uri  URI in one of the following formats:
  ///              host/path
  ///              service://host/path
  ///              service://
  ///              IPv4-host:service/path
  ///              [IPv6-host]:service/path
  ///              IPv6-host/path
  /// @throws  If parsing or host/service lookup fails.
  SocketAddress(std::string const &uri);

  /// Create a local/remote host address from given host and service name.
  /// @param  host  host name.
  /// @param  service  service number or name for well-known services.
  /// @throws  If host/service lookup fails.
  SocketAddress(std::string const &host,
                std::string const &service);

  /// Create a localhost address from given port number.
  /// @param  port  Port number.
  /// @throws  If parsing fails.
  SocketAddress(uint16_t port = 0U);

  /// Constructor for internal use.
  SocketAddress(std::shared_ptr<SocketAddressPriv> other);

  SocketAddress(SocketAddress const &other);
  SocketAddress(SocketAddress &&other) noexcept;
  ~SocketAddress();

  SocketAddress &operator=(SocketAddress const &other);
  SocketAddress &operator=(SocketAddress &&other) noexcept;

  /// Retrieve the host name of the address.
  std::string Host() const;

  /// Retrieve the service name of the address.
  std::string Service() const;

  /// Return whether the address is an IPv6 address (rather than an IPv4 one).
  bool IsV6() const;

  static std::vector<SocketAddress> GetLocalInterfaceAddresses();

  /// Pimpl getter for internal use.
  SocketAddressPriv const *Priv() const;

private:
  std::shared_ptr<SocketAddressPriv> m_priv;
};

bool operator<(SocketAddress const &lhs,
               SocketAddress const &rhs);

std::string to_string(SocketAddress const &addr);

} // namespace sockpuppet

#endif // SOCKET_ADDRESS_H
