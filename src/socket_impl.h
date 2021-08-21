#ifndef SOCKPUPPET_SOCKET_IMPL_H
#define SOCKPUPPET_SOCKET_IMPL_H

#include "address_impl.h" // for SockAddrView
#include "sockpuppet/address.h" // for Address
#include "sockpuppet/socket.h" // for SocketTcp
#include "winsock_guard.h" // for WinSockGuard

#ifdef _WIN32
# include <winsock2.h> // for SOCKET
#else
using SOCKET = int;
#endif // _WIN32

#include <cstddef> // for size_t
#include <memory> // for std::shared_ptr
#include <optional> // for std::optional
#include <utility> // for std::pair

namespace sockpuppet {

struct SocketImpl
{
  WinSockGuard guard;  ///< Guard to initialize socket subsystem on windows
  SOCKET fd;  ///< Socket file descriptor

  SocketImpl(int family,
             int type,
             int protocol);
  SocketImpl(SOCKET fd);
  SocketImpl(SocketImpl const &) = delete;
  SocketImpl(SocketImpl &&other) noexcept;
  virtual ~SocketImpl();

  // waits for readable
  virtual std::optional<size_t> Receive(char *data,
                                        size_t size,
                                        Duration timeout);
  // assumes a readable socket
  virtual size_t Receive(char *data,
                         size_t size);

  std::optional<std::pair<size_t, Address>>
  ReceiveFrom(char *data, size_t size, Duration timeout);
  std::pair<size_t, Address>
  ReceiveFrom(char *data, size_t size);

  virtual size_t Send(char const *data,
                      size_t size,
                      Duration timeout);
  size_t SendAll(char const *data,
                 size_t size);
  // waits for writable repeatedly and
  // sends the max amount of data within the user-provided timeout
  template<typename Deadline>
  size_t SendSome(char const *data,
                  size_t size,
                  Deadline deadline);
  // assumes a writable socket
  virtual size_t SendSome(char const *data,
                          size_t size);

  size_t SendTo(char const *data,
                size_t size,
                SockAddrView const &dstAddr,
                Duration timeout);
  size_t SendTo(char const *data,
                size_t size,
                SockAddrView const &dstAddr);

  void Bind(SockAddrView const &bindAddr);

  virtual void Connect(SockAddrView const &connectAddr);

  void Listen();

  std::optional<std::pair<SocketTcp, Address>>
  Accept(Duration timeout);
  virtual std::pair<SocketTcp, Address>
  Accept();

  virtual bool WaitReadable(Duration timeout);
  virtual bool WaitWritable(Duration timeout);

  void SetSockOptNonBlocking();
  void SetSockOptReuseAddr();
  void SetSockOptBroadcast();
  void SetSockOptNoSigPipe();
  size_t GetSockOptRcvBuf() const;
  std::shared_ptr<SockAddrStorage> GetSockName() const;
  std::shared_ptr<SockAddrStorage> GetPeerName() const;
};

} // namespace sockpuppet

#endif // SOCKPUPPET_SOCKET_IMPL_H
