#ifndef SOCKET_PRIV_H
#define SOCKET_PRIV_H

#include "socket.h" // for Socket
#include "socket_address_priv.h" // for SockAddr
#include "socket_guard.h" // for SocketGuard

#ifdef _WIN32
# include <winsock2.h> // for SOCKET
#else
# define SOCKET int
#endif // _WIN32

#include <cstddef> // for size_t

struct Socket::SocketPriv
{
  SocketGuard socketGuard;  ///< Guard to initialize socket subsystem on windows
  SOCKET fd;  ///< Socket file descriptor

  SocketPriv(int family,
             int type,
             int protocol);
  SocketPriv(SOCKET fd);
  ~SocketPriv();

  size_t Receive(char *data,
                 size_t size,
                 Socket::Time timeout);

  std::tuple<size_t, std::shared_ptr<SocketAddressStorage>>
  ReceiveFrom(char *data, size_t size, Time timeout);

  void Send(char const *data,
            size_t size,
            Socket::Time timeout);

  void SendTo(char const *data,
              size_t size,
              SockAddr const &dstAddr);

  void Bind(SockAddr const &bindAddr);

  void Connect(SockAddr const &connectAddr);

  std::tuple<std::unique_ptr<SocketPriv>,
             std::shared_ptr<SocketAddressStorage>>
  Listen(Time timeout);

  void SetSockOptReuseAddr();

  /// @return  0: timed out, <0: fd closed, >0: readable
  int SelectRead(Socket::Time timeout);

  /// @return  0: timed out, <0: fd closed, >0: writable
  int SelectWrite(Socket::Time timeout);
};

#endif // SOCKET_PRIV_H
