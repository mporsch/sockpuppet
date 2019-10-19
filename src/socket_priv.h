#ifndef SOCKPUPPET_SOCKET_PRIV_H
#define SOCKPUPPET_SOCKET_PRIV_H

#include "sockpuppet/socket.h" // for Socket
#include "address_priv.h" // for SockAddrView
#include "winsock_guard.h" // for WinSockGuard

#ifdef _WIN32
# include <winsock2.h> // for SOCKET
#else
# include <poll.h> // for pollfd
using SOCKET = int;
#endif // _WIN32

#include <cstddef> // for size_t
#include <memory> // for std::shared_ptr
#include <tuple> // for std::tuple

namespace sockpuppet {

struct Socket::SocketPriv
{
  WinSockGuard guard;  ///< Guard to initialize socket subsystem on windows
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
                 Duration timeout);

  std::tuple<size_t, std::shared_ptr<SockAddrStorage>>
  ReceiveFrom(char *data, size_t size, Duration timeout);

  void Send(char const *data,
            size_t size,
            Duration timeout);

  void SendTo(char const *data,
              size_t size,
              SockAddrView const &dstAddr);

  void Bind(SockAddrView const &sockAddr);

  void Connect(SockAddrView const &connectAddr);

  void Listen();

  std::tuple<std::unique_ptr<SocketPriv>,
             std::shared_ptr<SockAddrStorage>>
  Accept(Duration timeout);

  void SetSockOptReuseAddr();
  void SetSockOptBroadcast();
  void SetSockOpt(int id,
                  int value,
                  char const *errorMessage);
  size_t GetSockOptRcvBuf() const;
  std::shared_ptr<SockAddrStorage> GetSockName() const;
  std::shared_ptr<SockAddrStorage> GetPeerName() const;

  /// @return  0: timed out, <0: error, >0: readable/writable/closed
  int PollRead(Duration timeout) const;
  int PollWrite(Duration timeout) const;
};

} // namespace sockpuppet

#endif // SOCKPUPPET_SOCKET_PRIV_H
