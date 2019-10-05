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
  /// @throws  If accessing a moved-from instance.
  std::string Host() const;

  /// Retrieve the service name of the address.
  /// @throws  If accessing a moved-from instance.
  std::string Service() const;

  /// Retrieve the port number of the address.
  /// @throws  If accessing a moved-from instance.
  uint16_t Port() const;

  /// Return whether the address is an IPv6 address (rather than an IPv4 one).
  /// @throws  If accessing a moved-from instance.
  bool IsV6() const;

  /// Return a list of the OS's network interface addresses.
  static std::vector<SocketAddress> LocalAddresses();

  /// Pimpl getter for internal use.
  /// @throws  If accessing a moved-from instance.
  SocketAddressPriv const &Priv() const;

  /// @throws  If accessing a moved-from instance.
  bool operator<(SocketAddress const &other) const;

  /// @throws  If accessing a moved-from instance.
  bool operator==(SocketAddress const &other) const;

  /// @throws  If accessing a moved-from instance.
  bool operator!=(SocketAddress const &other) const;

private:
  std::shared_ptr<SocketAddressPriv> m_priv;
};

/// @throws  If accessing a moved-from instance.
std::string to_string(SocketAddress const &addr);

} // namespace sockpuppet

#endif // SOCKET_ADDRESS_H
