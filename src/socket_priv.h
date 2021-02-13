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
  bool isBlocking;  ///< socket blocking/non-blocking status, see NeedsPoll()

  SocketPriv(int family,
             int type,
             int protocol);
  SocketPriv(SOCKET fd);
  SocketPriv(SocketPriv const &) = delete;
  SocketPriv(SocketPriv &&other) noexcept;
  ~SocketPriv();

  size_t Receive(char *data,
                 size_t size,
                 Duration timeout);

  std::pair<size_t, std::shared_ptr<SockAddrStorage>>
  ReceiveFrom(char *data, size_t size, Duration timeout);

  size_t Send(char const *data,
              size_t size,
              Duration timeout);
  size_t SendSome(char const *data,
                  size_t size,
                  Duration timeout);

  size_t SendTo(char const *data,
                size_t size,
                SockAddrView const &dstAddr,
                Duration timeout);

  void Bind(SockAddrView const &bindAddr);

  void Connect(SockAddrView const &connectAddr);

  void Listen();

  std::pair<std::unique_ptr<SocketPriv>, std::shared_ptr<SockAddrStorage>>
  Accept(Duration timeout);

  void SetSockOptNonBlocking();
  void SetSockOptReuseAddr();
  void SetSockOptBroadcast();
  void SetSockOptNoSigPipe();
  void SetSockOpt(int id,
                  int value,
                  char const *errorMessage);
  size_t GetSockOptRcvBuf() const;
  std::shared_ptr<SockAddrStorage> GetSockName() const;
  std::shared_ptr<SockAddrStorage> GetPeerName() const;

  /// @return  false: timed out, true: readable/writable/closed
  bool PollReadable(Duration timeout);
  bool PollWritable(Duration timeout);

  /// each socket is created in blocking mode:
  ///   no poll is needed before send/receive calls
  ///   TCP send always transmits the whole buffer at once
  /// if a timeout >=0 is given in any send/receive call,
  /// the socket permanently switches to non-blocking mode:
  ///   poll is needed before each send/receive call
  ///   TCP send may transmit a buffer partially
  bool NeedsPoll(Duration timeout);
};

} // namespace sockpuppet

#endif // SOCKPUPPET_SOCKET_PRIV_H
