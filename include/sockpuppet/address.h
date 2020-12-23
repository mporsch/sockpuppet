#ifndef SOCKPUPPET_ADDRESS_H
#define SOCKPUPPET_ADDRESS_H

#include <cstdint> // for uint16_t
#include <memory> // for std::shared_ptr
#include <string> // for std::string
#include <vector> // for std::vector

namespace sockpuppet {

struct Address
{
  /// Create a local/remote host address from given URI.
  /// @param  uri  URI in one of the following formats:
  ///              host/path
  ///              service://host/path
  ///              service://
  ///              IPv4-host:service/path
  ///              [IPv6-host]:service/path
  ///              IPv6-host/path
  /// @throws  If parsing or host/service lookup fails.
  Address(std::string const &uri);

  /// Create a local/remote host address from given host and service name.
  /// @param  host  host name.
  /// @param  service  service number or name for well-known services.
  /// @throws  If host/service lookup fails.
  Address(std::string const &host,
          std::string const &service);

  /// Create a localhost address from given port number.
  /// @param  port  Port number; 0 can be used for
  ///               binding to an OS-assigned port.
  /// @throws  If parsing fails.
  Address(uint16_t port = 0U);

  Address(Address const &other);
  Address(Address &&other) noexcept;
  ~Address();
  Address &operator=(Address const &other);
  Address &operator=(Address &&other) noexcept;
  bool operator<(Address const &other) const;
  bool operator==(Address const &other) const;
  bool operator!=(Address const &other) const;

  /// Retrieve the host name of the address.
  std::string Host() const;

  /// Retrieve the service name of the address.
  std::string Service() const;

  /// Retrieve the port number of the address.
  uint16_t Port() const;

  /// Return whether the address is an IPv6 address (rather than an IPv4 one).
  bool IsV6() const;

  /// Return a list of the OS's network interface addresses.
  static std::vector<Address> LocalAddresses();

public: // for internal use
  /// Pimpl to hide away the OS-specifics.
  struct AddressPriv;
  std::shared_ptr<AddressPriv> priv;

  Address(std::shared_ptr<AddressPriv> other);
};

/// String format address as "host:port"
std::string to_string(Address const &addr);

} // namespace sockpuppet

#endif // SOCKPUPPET_ADDRESS_H
