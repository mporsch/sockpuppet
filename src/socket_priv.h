#ifndef SOCKPUPPET_SOCKET_PRIV_H
#define SOCKPUPPET_SOCKET_PRIV_H

#include "sockpuppet/socket.h" // for Socket
#include "address_priv.h" // for SockAddrView
#include "winsock_guard.h" // for WinSockGuard

#ifdef _WIN32
# include <winsock2.h> // for SOCKET
#else
using SOCKET = int;
#endif // _WIN32

#include <cstddef> // for size_t
#include <memory> // for std::shared_ptr
#include <utility> // for std::pair

namespace sockpuppet {

struct SocketPriv
{
  WinSockGuard guard;  ///< Guard to initialize socket subsystem on windows
  SOCKET fd;  ///< Socket file descriptor

  SocketPriv(int family,
             int type,
             int protocol);
  SocketPriv(SOCKET fd);
  SocketPriv(SocketPriv const &) = delete;
  SocketPriv(SocketPriv &&other) noexcept;
  ~SocketPriv();

  std::optional<size_t> Receive(char *data,
                                size_t size,
                                Duration timeout);
  size_t Receive(char *data,
                 size_t size);

  std::optional<std::pair<size_t, std::shared_ptr<SockAddrStorage>>>
  ReceiveFrom(char *data, size_t size, Duration timeout);
  std::pair<size_t, std::shared_ptr<SockAddrStorage>>
  ReceiveFrom(char *data, size_t size);

  size_t Send(char const *data,
              size_t size,
              Duration timeout);
  size_t SendAll(char const *data,
                 size_t size);
  size_t SendSome(char const *data,
                  size_t size,
                  Duration timeout);
  size_t SendSome(char const *data,
                  size_t size);

  size_t SendTo(char const *data,
                size_t size,
                SockAddrView const &dstAddr,
                Duration timeout);

  void Bind(SockAddrView const &bindAddr);

  void Connect(SockAddrView const &connectAddr);

  void Listen();

  std::optional<std::pair<std::unique_ptr<SocketPriv>, std::shared_ptr<SockAddrStorage>>>
  Accept(Duration timeout);
  std::pair<std::unique_ptr<SocketPriv>, std::shared_ptr<SockAddrStorage>>
  Accept();

  void SetSockOptReuseAddr();
  void SetSockOptBroadcast();
  void SetSockOptNoSigPipe();
  size_t GetSockOptRcvBuf() const;
  std::shared_ptr<SockAddrStorage> GetSockName() const;
  std::shared_ptr<SockAddrStorage> GetPeerName() const;
};

} // namespace sockpuppet

#endif // SOCKPUPPET_SOCKET_PRIV_H
