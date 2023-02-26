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

struct View : public
    #ifdef _WIN32
      WSABUF
    #else // _WIN32
      iovec
    #endif // _WIN32
{
  View(char const *data, size_t size);

  char const *Data() const;
  size_t Size() const;

  void Advance(size_t count);
};
#ifdef _WIN32
static_assert(sizeof(View) == sizeof(WSABUF), "mismatching wrapper size");
#else // _WIN32
static_assert(sizeof(View) == sizeof(iovec), "mismatching wrapper size");
#endif // _WIN32

using ViewsBackend = std::vector<View>;

struct Views : public ViewsBackend
{
  Views(char const *data, size_t size);
  Views(std::initializer_list<std::string_view>);

  void Advance(size_t count);
  size_t OverallSize() const;
};

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

  virtual size_t Send(Views &, Duration timeout);
  size_t SendAll(Views &);
  // waits for writable repeatedly and
  // sends the max amount of data within the user-provided timeout
  template<typename Deadline>
  size_t SendSome(Views &, Deadline deadline);
  // assumes a writable socket
  virtual size_t SendSome(Views &);

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
