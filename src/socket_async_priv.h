#ifndef SOCKPUPPET_SOCKET_ASYNC_PRIV_H
#define SOCKPUPPET_SOCKET_ASYNC_PRIV_H

#include "address_priv.h" // for AddressPriv
#include "socket_buffered_priv.h" // for SocketBufferedPriv
#include "sockpuppet/socket_async.h" // for SocketAsync

#ifdef _WIN32
# include <winsock2.h> // for pollfd
#else
# include <poll.h> // for pollfd
#endif // _WIN32

#include <atomic> // for std::atomic
#include <deque> // for std::deque
#include <future> // for std::future
#include <mutex> // for std::mutex
#include <queue> // for std::queue
#include <tuple> // for std::tuple
#include <vector> // for std::vector

namespace sockpuppet {

using AddressShared = std::shared_ptr<Address::AddressPriv>;
using DriverShared = std::shared_ptr<SocketDriver::SocketDriverPriv>;
using ToDoShared = std::shared_ptr<ToDo::ToDoPriv>;

// list of ToDo elements sorted by scheduled time
struct ToDos : public std::deque<ToDoShared>
{
  void Insert(ToDoShared todo);
  void Remove(ToDo::ToDoPriv *todo);
  void Move(ToDoShared todo, TimePoint when);

  template<typename Pred>
  std::deque<ToDoShared>::iterator Find(Pred);
};

struct SocketDriver::SocketDriverPriv
{
  using SocketRef = std::reference_wrapper<SocketAsyncPriv>;

  // StepGuard and StopGuard perform a handshake to obtain stepMtx
  // with pauseMtx used to force Step() to yield

  struct StepGuard
  {
    std::unique_lock<std::recursive_mutex> stepLock;
    std::unique_lock<std::mutex> pauseLock;

    StepGuard(SocketDriverPriv &priv);
    StepGuard(StepGuard const &) = delete;
    StepGuard(StepGuard &&) = delete;
    ~StepGuard();
    StepGuard &operator=(StepGuard const &) = delete;
    StepGuard &operator=(StepGuard &&) = delete;
  };

  struct PauseGuard
  {
    std::unique_lock<std::recursive_mutex> stepLock;

    PauseGuard(SocketDriverPriv &priv);
    PauseGuard(PauseGuard const &) = delete;
    PauseGuard(PauseGuard &&) = delete;
    ~PauseGuard();
    PauseGuard &operator=(PauseGuard const &) = delete;
    PauseGuard &operator=(PauseGuard &&) = delete;
  };

  /// internal signalling pipe for cancelling Step()
  AddressShared pipeToAddr;
  SocketPriv pipeFrom;
  SocketPriv pipeTo;

  std::recursive_mutex stepMtx;
  std::mutex pauseMtx;
  ToDos todos; // guarded by stepMtx
  std::vector<SocketRef> sockets; // guarded by stepMtx
  std::vector<pollfd> pfds; // front element belongs to internal signalling pipe; guarded by stepMtx

  std::atomic<bool> shouldStop; ///< flag for cancelling Run()

  SocketDriverPriv();
  SocketDriverPriv(SocketDriverPriv const &) = delete;
  SocketDriverPriv(SocketDriverPriv &&) = delete;
  ~SocketDriverPriv();
  SocketDriverPriv &operator=(SocketDriverPriv const &) = delete;
  SocketDriverPriv &operator=(SocketDriverPriv &&) = delete;

  void Step(Duration timeout);
  Duration StepTodos(Duration timeout);
  void StepFds(Duration timeout);

  void Run();
  void Stop();

  // interface for ToDoPriv
  void ToDoInsert(ToDoShared todo);
  void ToDoRemove(ToDo::ToDoPriv *todo);
  void ToDoMove(ToDoShared todo, TimePoint when);

  // interface for SocketAsyncPriv
  void AsyncRegister(SocketAsyncPriv &sock);
  void AsyncUnregister(SOCKET fd);
  void AsyncWantSend(SOCKET fd);

  void Bump();
  void Unbump();

  void DoOneFdTask();
};

struct ToDo::ToDoPriv : public std::enable_shared_from_this<ToDoPriv>
{
  std::weak_ptr<SocketDriver::SocketDriverPriv> driver;
  std::function<void()> what;
  TimePoint when;

  ToDoPriv(DriverShared &driver, std::function<void()> what);
  ToDoPriv(DriverShared &driver, std::function<void()> what, TimePoint when);
  ToDoPriv(ToDoPriv const &) = delete;
  ToDoPriv(ToDoPriv &&) = delete;
  ~ToDoPriv();
  ToDoPriv &operator=(ToDoPriv const &) = delete;
  ToDoPriv &operator=(ToDoPriv &&) = delete;

  void Cancel();

  void Shift(TimePoint when);
};

struct SocketAsyncPriv : public SocketBufferedPriv
{
  struct Handlers
  {
    ReceiveHandler receive;
    ReceiveFromHandler receiveFrom;
    ConnectHandler connect;
    DisconnectHandler disconnect;
  };
  using SendQElement = std::tuple<std::promise<void>, BufferPtr>;
  using SendQ = std::queue<SendQElement>;
  using SendToQElement = std::tuple<std::promise<void>, BufferPtr, AddressShared>;
  using SendToQ = std::queue<SendToQElement>;

  std::weak_ptr<SocketDriver::SocketDriverPriv> driver;
  Handlers handlers;
  std::mutex sendQMtx;
  SendQ sendQ;
  SendToQ sendToQ;
  std::shared_ptr<SockAddrStorage> peerAddr;

  SocketAsyncPriv(SocketPriv &&sock, DriverShared &driver, Handlers handlers);
  SocketAsyncPriv(SocketBufferedPriv &&buff, DriverShared &driver, Handlers handlers);
  SocketAsyncPriv(SocketAsyncPriv const &) = delete;
  SocketAsyncPriv(SocketAsyncPriv &&) = delete;
  ~SocketAsyncPriv();
  SocketAsyncPriv &operator=(SocketAsyncPriv const &) = delete;
  SocketAsyncPriv &operator=(SocketAsyncPriv &&) = delete;

  std::future<void> Send(BufferPtr &&buffer);
  std::future<void> SendTo(BufferPtr &&buffer, AddressShared dstAddr);

  template<typename Queue, typename... Args>
  std::future<void> DoSend(Queue &q, Args&&... args);

  // in thread context of SocketDriverPriv
  void DriverDoFdTaskReadable();

  /// @return  true if there is no more data to send, false otherwise
  bool DriverDoFdTaskWritable();
  void DriverDoSend(SendQElement &t);
  void DriverDoSendTo(SendToQElement &t);

  void DriverDoFdTaskError();
};

} // namespace sockpuppet

#endif // SOCKPUPPET_SOCKET_ASYNC_PRIV_H
