#ifndef SOCKPUPPET_SOCKET_ASYNC_H
#define SOCKPUPPET_SOCKET_ASYNC_H

#include "sockpuppet/address.h" // for Address
#include "sockpuppet/socket.h" // for Duration
#include "sockpuppet/socket_buffered.h" // for BufferPtr

#include <functional> // for std::function
#include <future> // for std::future
#include <memory> // for std::unique_ptr

namespace sockpuppet {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

/// Driver (event loop / scheduler / context) that runs multiple attached socket
/// and ToDo classes and may be driven by a dedicated thread or stepped iteratively.
/// @note  Thread-safe with respect to connected tasks and sockets; these
///        can safely be managed irrespective of concurrent driver state.
struct Driver
{
  /// Create a driver that can be passed to sockets or ToDos to attach to.
  /// @throws  If creating the internal event signalling fails.
  Driver();

  /// Run one iteration on the attached sockets.
  /// @param  timeout  Maximum allowed time to use; non-null allows
  ///                  blocking if all attached sockets are idle,
  ///                  a negative value allows unlimited blocking.
  /// @throws  If the internal event handling fails.
  /// @note  Does not provide an accurate time source to wait for;
  ///        use \ref ToDo instead.
  void Step(Duration timeout = Duration(-1));

  /// Continuously run the attached sockets.
  /// @throws  If the internal event handling fails.
  /// @note  Blocking call. Returns only after Stop() from another thread.
  void Run();

  /// Cancel the continuously running Run() method.
  /// @throws  If the internal event signalling fails.
  void Stop();

  Driver(Driver const &) = delete;
  Driver(Driver &&other) noexcept;
  ~Driver();
  Driver &operator=(Driver const &) = delete;
  Driver &operator=(Driver &&other) noexcept;

  /// Bridge to hide away the OS-specifics.
  struct DriverImpl;
  std::shared_ptr<DriverImpl> impl;
};

/// Scheduled task to be executed later.
/// May be cancelled or shifted to (re)run at a different time.
struct ToDo
{
  /// Create a task to be scheduled later.
  /// @param  driver  Driver to run the task.
  /// @param  task  Task to execute after being scheduled.
  ToDo(Driver &driver,
       std::function<void()> task);

  /// Create and schedule a task to be executed later.
  /// @param  driver  Driver to run the task.
  /// @param  task  Task to execute on time given by \ref when.
  /// @param  when  Point in time when to execute the task. If the time
  ///               has already passed, the task will be executed asap.
  /// @note  The object does not need to be kept if no
  ///        subsequent \ref Cancel or \ref Shift is intended.
  ToDo(Driver &driver,
       std::function<void()> task,
       TimePoint when);

  /// Create and schedule a task to be executed later.
  /// @param  driver  Driver to run the task.
  /// @param  task  Task to execute on time given by \ref delay.
  /// @param  delay  Point in time from now when to execute the task. If the delay
  ///         is less or equal zero, the task will be executed asap.
  /// @note  The object does not need to be kept if no
  ///        subsequent \ref Cancel or \ref Shift is intended.
  ToDo(Driver &driver,
       std::function<void()> task,
       Duration delay);

  /// Cancel a pending task.
  /// @note  Cancelling an already executed task has no effect.
  /// @note  A scheduled task is not cancelled on object destruction;
  ///        manual cancel is required to fully release a not-yet executed task.
  void Cancel();

  /// Shift task execution to (re)run at a different time.
  /// @param  when  Point in time when to execute the task. If the time
  ///               has already passed, the task will be executed asap.
  void Shift(TimePoint when);

  /// Shift task execution to (re)run at a different time.
  /// @param  delay  Point in time from now when to execute the task. If the delay
  ///         is less or equal zero, the task will be executed asap.
  void Shift(Duration delay);

  ToDo(ToDo const &) = delete;
  ToDo(ToDo &&other) noexcept;
  ~ToDo();
  ToDo &operator=(ToDo const &) = delete;
  ToDo &operator=(ToDo &&other) noexcept;

  /// Bridge to implementation instance shared with driver.
  struct ToDoImpl;
  std::shared_ptr<ToDoImpl> impl;
};

struct SocketAsyncImpl;

/// Callback for UDP received data.
/// @param  Received data buffer borrowed from socket.
///         Zero-size receipt is valid in UDP (header-only packet).
/// @param  Receipt source address.
using ReceiveFromHandler = std::function<void(BufferPtr, Address)>;

/// Callack for TCP received data from connected peer.
/// @param  Received data buffer borrowed from socket.
///         Zero-size receipt cannot happen in TCP.
using ReceiveHandler = std::function<void(BufferPtr)>;

/// Callback for accepted incoming TCP connections.
/// @param  Local socket connected to peer.
/// @param  Address of peer connected to local socket.
/// @note  Obtained basic socket can be used as-is or may be upgraded
///        to async socket with same or different driver.
using ConnectHandler = std::function<void(SocketTcp, Address)>;

/// Callback for TCP peer disconnect.
/// @param  Address of peer that just disconnected from local socket.
///         Matches connect address the socket was created with or
///         obtained peer address of incoming connection.
/// @param  Error/reason message.
/// @note  After peer disconnect the socket is invalid and should be released.
using DisconnectHandler = std::function<void(Address, char const *)>;

/// UDP (unreliable communication) socket class that adds an interface for
/// an external socket driver to the buffered UDP class.
struct SocketUdpAsync
{
  /// Create a UDP socket driven by given socket driver.
  /// @param  buff  Buffered UDP socket to augment.
  /// @param  driver  Socket driver to run the socket.
  /// @param  handleReceiveFrom  (Bound) function to call on receipt.
  /// @throws  If an invalid handler is provided.
  SocketUdpAsync(SocketUdpBuffered &&buff,
                 Driver &driver,
                 ReceiveFromHandler handleReceiveFrom);

  /// Enqueue data to unreliably send to address.
  /// @param  buffer  Borrowed buffer to enqueue for send and release after completition.
  ///                 Create using your own BufferPool.
  /// @param  dstAddress  Address to send to; must match
  ///                     IP family of bound address.
  /// @return  Future object to fulfill when data was actually sent.
  std::future<void> SendTo(BufferPtr &&buffer,
                           Address const &dstAddress);

  /// Get the local (bound-to) address of the socket.
  /// @throws  If the address lookup fails.
  Address LocalAddress() const;

  SocketUdpAsync(SocketUdpAsync const &other) = delete;
  SocketUdpAsync(SocketUdpAsync &&other) noexcept;
  ~SocketUdpAsync();
  SocketUdpAsync &operator=(SocketUdpAsync const &other) = delete;
  SocketUdpAsync &operator=(SocketUdpAsync &&other) noexcept;

  /// Bridge to hide away the OS-specifics.
  std::unique_ptr<SocketAsyncImpl> impl;
};

/// TCP (reliable communication) socket class that adds an interface for
/// an external socket driver to the buffered TCP client class.
struct SocketTcpAsync
{
  /// Create a TCP client socket driven by given socket driver.
  /// @param  buff  Buffered TCP client socket to augment.
  /// @param  driver  Socket driver to run the socket.
  /// @param  handleReceive  (Bound) function to call on receipt from
  ///                        connected peer.
  /// @param  handleDisconnect  (Bound) function to call when socket was
  ///                           disconnected and has become invalid.
  /// @throws  If an invalid handler is provided.
  SocketTcpAsync(SocketTcpBuffered &&buff,
                 Driver &driver,
                 ReceiveHandler handleReceive,
                 DisconnectHandler handleDisconnect);

  /// Enqueue data to reliably send to connected peer.
  /// @param  buffer  Borrowed buffer to enqueue for send and release after completition.
  ///                 Create using your own BufferPool.
  /// @return  Future object to fulfill when data was actually sent.
  std::future<void> Send(BufferPtr &&buffer);

  /// Get the local (bound-to) address of the socket.
  /// @throws  If the address lookup fails.
  Address LocalAddress() const;

  /// Get the remote peer address of the socket.
  /// @throws  If the address lookup fails.
  Address PeerAddress() const;

  SocketTcpAsync(SocketTcpAsync const &other) = delete;
  SocketTcpAsync(SocketTcpAsync &&other) noexcept;
  ~SocketTcpAsync();
  SocketTcpAsync &operator=(SocketTcpAsync const &other) = delete;
  SocketTcpAsync &operator=(SocketTcpAsync &&other) noexcept;

  /// Bridge to hide away the OS-specifics.
  std::unique_ptr<SocketAsyncImpl> impl;
};

/// TCP (reliable communication) socket class that adds an interface for
/// an external socket driver to the regular TCP server class.
struct AcceptorAsync
{
  /// Create a TCP server socket driven by given socket driver.
  /// @param  sock  TCP server socket to augment.
  /// @param  driver  Socket driver to run the socket.
  /// @param  handleConnect  (Bound) function to call when a TCP client connects.
  /// @throws  If an invalid handler is provided.
  AcceptorAsync(Acceptor &&sock,
                Driver &driver,
                ConnectHandler handleConnect);

  /// Get the local (bound-to) address of the socket.
  /// @throws  If the address lookup fails.
  Address LocalAddress() const;

  AcceptorAsync(AcceptorAsync const &other) = delete;
  AcceptorAsync(AcceptorAsync &&other) noexcept;
  ~AcceptorAsync();
  AcceptorAsync &operator=(AcceptorAsync const &other) = delete;
  AcceptorAsync &operator=(AcceptorAsync &&other) noexcept;

  /// Bridge to hide away the OS-specifics.
  std::unique_ptr<SocketAsyncImpl> impl;
};

// compatibility with legacy names
using SocketTcpAsyncClient [[deprecated]] = SocketTcpAsync;
using SocketTcpAsyncServer [[deprecated]] = AcceptorAsync;

} // namespace sockpuppet

#endif // SOCKPUPPET_SOCKET_ASYNC_H
