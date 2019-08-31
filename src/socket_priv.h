#ifndef SOCKET_PRIV_H
#define SOCKET_PRIV_H

#include "socket.h" // for Socket
#include "socket_address_priv.h" // for SockAddrView
#include "socket_guard.h" // for SocketGuard

#ifdef _WIN32
# include <Winsock2.h> // for SOCKET
#else
# include <sys/select.h> // for fd_set
using SOCKET = int;
#endif // _WIN32

#include <cstddef> // for size_t
#include <memory> // for std::shared_ptr
#include <tuple> // for std::tuple

namespace sockpuppet {

struct Socket::SocketPriv
{
  SocketGuard socketGuard;  ///< Guard to initialize socket subsystem on windows
  SOCKET fd;  ///< Socket file descriptor

  SocketPriv(int family,
             int type,
             int protocol);
  SocketPriv(SOCKET fd);
  SocketPriv(SocketPriv const &) = delete;
  SocketPriv(SocketPriv &&other) noexcept;
  virtual ~SocketPriv();

  size_t Receive(char *data,
                 size_t size,
                 Time timeout);

  std::tuple<size_t, std::shared_ptr<SocketAddress::SocketAddressPriv>>
  ReceiveFrom(char *data, size_t size, Time timeout);

  void Send(char const *data,
            size_t size,
            Time timeout);

  void SendTo(char const *data,
              size_t size,
              SockAddrView const &dstAddr);

  void Bind(SockAddrView const &sockAddr);

  void Connect(SockAddrView const &connectAddr);

  void Listen();

  std::tuple<std::unique_ptr<SocketPriv>,
             std::shared_ptr<SocketAddress::SocketAddressPriv>>
  Accept(Time timeout);

  void SetSockOptReuseAddr();
  void SetSockOptBroadcast();
  void SetSockOpt(int id,
                  int value,
                  char const *name);
  int GetSockOptRcvBuf() const;
  std::shared_ptr<SockAddrStorage> GetSockName() const;
  std::shared_ptr<SockAddrStorage> GetPeerName() const;

  /// @return  0: timed out, <0: fd closed, >0: readable/writable
  int SelectRead(Time timeout);
  int SelectWrite(Time timeout);
  int Select(fd_set *rfds, fd_set *wfds, Time timeout);

  static timeval ToTimeval(Time time);
};

} // namespace sockpuppet

#endif // SOCKET_PRIV_H
