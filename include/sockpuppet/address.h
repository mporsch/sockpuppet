#ifndef SOCKPUPPET_ADDRESS_H
#define SOCKPUPPET_ADDRESS_H

#include <cstdint> // for uint16_t
#include <functional> // for std::hash
#include <memory> // for std::shared_ptr
#include <string> // for std::string
#include <vector> // for std::vector

namespace sockpuppet {

struct Address
{
  /// Create a local/remote host address from given URI.
  /// @param  uri  URI in one of the following formats:
  ///              service://host/path
  ///              host:service/path
  ///              [IPv6-host]:service/path
  ///              host/path
  ///              service://
  /// @throws  If parsing or host/service lookup fails.
  Address(std::string const &uri);

  /// Create a local/remote host address from given host and service name.
  /// @param  host  host name.
  /// @param  service  service number or name for well-known services.
  /// @throws  If host/service lookup fails.
  Address(std::string const &host,
          std::string const &service);

  /// Create a local/remote host address from given reference address and
  /// override port number.
  /// @param  other  reference address.
  /// @param  port  Port number; 0 can be used for
  ///               binding to an OS-assigned port.
  Address(Address const &other,
          uint16_t port);

  /// Create a localhost address from given port number.
  /// @param  port  Port number; 0 can be used for
  ///               binding to an OS-assigned port.
  /// @throws  If parsing fails.
  Address(uint16_t port = 0U);

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

  struct AddressImpl;
  Address(std::shared_ptr<AddressImpl> other);
  Address(Address const &other);
  Address(Address &&other) noexcept;
  ~Address();
  Address &operator=(Address const &other);
  Address &operator=(Address &&other) noexcept;
  bool operator<(Address const &other) const;
  bool operator==(Address const &other) const;
  bool operator!=(Address const &other) const;

  /// Bridge to hide away the OS-specifics.
  std::shared_ptr<AddressImpl> impl;
};

/// String format address as "host:port"
std::string to_string(Address const &addr);

} // namespace sockpuppet

namespace std {

template<>
struct hash<sockpuppet::Address>
{
  size_t operator()(sockpuppet::Address const &addr) const;
};

} // namespace std

#endif // SOCKPUPPET_ADDRESS_H
