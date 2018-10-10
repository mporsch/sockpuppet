#include "socket.h"
#include "socket_address_priv.h" // for SocketAddress::SocketAddressPriv

#include <arpa/inet.h> // for IPPROTO_UDP
#include <sys/socket.h> // for ::socket
#include <unistd.h> // for ::close

#include <stdexcept> // for std::runtime_error

Socket::Socket(SocketAddress const &address)
  : m_fd(::socket(address.priv->info->ai_family, SOCK_DGRAM, IPPROTO_UDP))
{
  if(m_fd < 0) {
    throw std::runtime_error("failed to create socket");
  } else {
    if(bind(m_fd, address.priv->info->ai_addr, address.priv->info->ai_addrlen)) {
      throw std::runtime_error("failed to bind socket on address " + std::to_string(address));
    }
  }
}

Socket::~Socket()
{
  ::close(m_fd);
}

void Socket::Transmit(char const *data, size_t size)
{

}

void Socket::Receive(char *data, size_t size, long timeout)
{

}
