#ifndef SOCKET_ADDRESS_H
#define SOCKET_ADDRESS_H

#include <cstdint> // for uint16_t
#include <memory> // for std::shared_ptr
#include <string> // for std::string

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

  /// Create a localhost address from given port number.
  /// @param  port  Port number.
  /// @throws  If parsing fails.
  SocketAddress(uint16_t port = 0U);

  /// Constructor for internal use.
  SocketAddress(std::shared_ptr<SocketAddressPriv> &&other);

  SocketAddress(SocketAddress const &other);
  SocketAddress(SocketAddress &&other);
  ~SocketAddress();

  SocketAddress &operator=(SocketAddress const &other);
  SocketAddress &operator=(SocketAddress &&other);

  /// Pimpl getter for internal use.
  SocketAddressPriv const *Priv() const;

private:
  std::shared_ptr<SocketAddressPriv> m_priv;
};

namespace std {
  std::string to_string(SocketAddress const &addr);
}

#endif // SOCKET_ADDRESS_H
