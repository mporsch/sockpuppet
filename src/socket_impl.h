#ifndef SOCKPUPPET_SOCKET_IMPL_H
#define SOCKPUPPET_SOCKET_IMPL_H

#include "address_impl.h" // for SockAddrView
#include "sockpuppet/address.h" // for Address
#include "sockpuppet/socket.h" // for SocketTcp
#include "wait.h" // for DeadlineLimited
#include "winsock_guard.h" // for WinSockGuard

#ifdef _WIN32
# include <winsock2.h> // for SOCKET
#else
using SOCKET = int;
#endif // _WIN32

#include <cstddef> // for size_t
#include <memory> // for std::shared_ptr
#include <optional> // for std::optional
#include <string_view> // for std::string_view
#include <utility> // for std::pair

namespace sockpuppet {

using ViewBackend =
#ifdef _WIN32
  WSABUF;
#else // _WIN32
  iovec;
#endif // _WIN32

struct View : public ViewBackend
{
  View(char const *data, size_t size);
  View(std::string_view);

  char const *Data() const;
  size_t Size() const;

  void Advance(size_t count);
};
static_assert(sizeof(View) == sizeof(ViewBackend), "mismatching wrapper size");

using ViewsBackend = std::vector<View>;

struct Views : public ViewsBackend
{
  Views(char const *data, size_t size);
  Views(std::initializer_list<std::string_view>);

  void Advance(size_t count);
  size_t OverallSize() const;
};

struct SocketImpl
#ifndef SOCKPUPPET_WITH_TLS
    final // without SocketTlsImpl the compiler may optimize away the unused vtable
#endif // SOCKPUPPET_WITH_TLS
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

  // waits for writable (repeatedly if needed)
  virtual size_t Send(char const *data,
                      size_t size,
                      Duration timeout);
  virtual size_t Send(Views &,
                      Duration timeout);
  // assumes a writable socket
  virtual size_t SendSome(char const *data,
                          size_t size);
  virtual size_t SendSome(Views &);

  size_t SendTo(Views &,
                SockAddrView const &dstAddr,
                Duration timeout);
  size_t SendTo(Views &,
                SockAddrView const &dstAddr);

  void Bind(SockAddrView const &bindAddr);

  virtual void Connect(SockAddrView const &connectAddr);

  void Listen();

  std::optional<std::pair<SocketTcp, Address>>
  Accept(Duration timeout);
  virtual std::pair<SocketTcp, Address> Accept();

  void SetSockOptNonBlocking();
  void SetSockOptReuseAddr();
  void SetSockOptBroadcast();
  void SetSockOptNoSigPipe();
  size_t GetSockOptRcvBuf() const;
  std::shared_ptr<SockAddrStorage> GetSockName() const;
  std::shared_ptr<SockAddrStorage> GetPeerName() const;

  virtual void DriverQuery(short &events);
  virtual void DriverPending();
};

// assumes a readable socket
size_t ReceiveNow(SOCKET fd, char *data, size_t size);

// wait for readable and read what is available
std::optional<size_t> Receive(SOCKET fd, char *data, size_t size, Duration timeout);

// assumes a writable socket
size_t SendNow(SOCKET fd, Views &);

// send everything no matter how long it takes
size_t SendAll(SOCKET fd, Views &);

// send what can be sent now without blocking
size_t SendTry(SOCKET fd, Views &);

// waits for writable (repeatedly) and sends the max amount of data within the deadline
size_t SendSome(SOCKET fd, Views &, DeadlineLimited &deadline);

std::pair<SOCKET, Address> Accept(SOCKET fd);

} // namespace sockpuppet

#endif // SOCKPUPPET_SOCKET_IMPL_H
