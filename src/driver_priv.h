#ifndef SOCKPUPPET_DRIVER_PRIV_H
#define SOCKPUPPET_DRIVER_PRIV_H

#include "socket_priv.h" // for SocketPriv
#include "sockpuppet/address.h" // for Address
#include "sockpuppet/socket_async.h" // for Driver
#include "todo_priv.h" // for ToDos

#ifdef _WIN32
# include <winsock2.h> // for pollfd
#else
# include <poll.h> // for pollfd
#endif // _WIN32

#include <atomic> // for std::atomic
#include <functional> // for std::reference_wrapper
#include <memory> // for std::shared_ptr
#include <mutex> // for std::mutex
#include <vector> // for std::vector

namespace sockpuppet {

struct Driver::DriverPriv
{
  using AddressShared = std::shared_ptr<Address::AddressPriv>;
  using SocketRef = std::reference_wrapper<SocketAsyncPriv>;

  // StepGuard and StopGuard perform a handshake to obtain stepMtx
  // with pauseMtx used to force Step() to yield

  struct StepGuard
  {
    std::unique_lock<std::recursive_mutex> stepLock;
    std::unique_lock<std::mutex> pauseLock;

    StepGuard(DriverPriv &priv);
    StepGuard(StepGuard const &) = delete;
    StepGuard(StepGuard &&) = delete;
    ~StepGuard();
    StepGuard &operator=(StepGuard const &) = delete;
    StepGuard &operator=(StepGuard &&) = delete;
  };

  struct PauseGuard
  {
    std::unique_lock<std::recursive_mutex> stepLock;

    PauseGuard(DriverPriv &priv);
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

  DriverPriv();
  DriverPriv(DriverPriv const &) = delete;
  DriverPriv(DriverPriv &&) = delete;
  ~DriverPriv();
  DriverPriv &operator=(DriverPriv const &) = delete;
  DriverPriv &operator=(DriverPriv &&) = delete;

  void Step(Duration timeout);
  template<typename Deadline>
  Duration StepTodos(Deadline);
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

} // namespace sockpuppet

#endif // SOCKPUPPET_DRIVER_PRIV_H
