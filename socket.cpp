#include "socket.h"
#include "socket_address_priv.h" // for SocketAddress::SocketAddressPriv

#ifdef _WIN32
# include <winsock2.h> // for ::socket
# pragma comment(lib, "Ws2_32.lib")
#else
# include <arpa/inet.h> // for IPPROTO_UDP
# include <sys/socket.h> // for ::socket
# include <unistd.h> // for ::close
#endif // _WIN32

#include <stdexcept> // for std::runtime_error

Socket::Socket(SocketAddress const &bindAddress)
  : m_fd(::socket(bindAddress.priv->info->ai_family, SOCK_DGRAM, IPPROTO_UDP))
{
  if(m_fd < 0) {
    throw std::runtime_error("failed to create socket: "
                             + std::to_string(errno));
  } else {
    if(bind(m_fd,
            bindAddress.priv->info->ai_addr,
            bindAddress.priv->info->ai_addrlen)) {
      throw std::runtime_error("failed to bind socket on address "
                               + std::to_string(bindAddress) + ": "
                               + std::to_string(errno));
    }
  }
}

Socket::~Socket()
{
#ifdef _WIN32
  closesocket(m_fd);
#else
  ::close(m_fd);
#endif _WIN32
}

void Socket::Transmit(char const *data, size_t size,
  SocketAddress const &dstAddress)
{
  for(auto info = dstAddress.priv->info.get();
      info != nullptr;
      info = info->ai_next) {
    if(size != ::sendto(m_fd, data, size, 0,
        info->ai_addr, info->ai_addrlen)) {
      throw std::runtime_error("failed to transmit: "
                               + std::to_string(errno));
    }
  }
}

size_t Socket::Receive(char *data, size_t size)
{
  return ::recv(m_fd, data, size, 0);
}
