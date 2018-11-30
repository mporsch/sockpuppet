#ifndef SOCKET_ASYNC_PRIV_H
#define SOCKET_ASYNC_PRIV_H

#include "socket_address_priv.h" // for SocketAddress::SocketAddressPriv
#include "socket_async.h" // for SocketAsync
#include "socket_buffered_priv.h" // for SocketBuffered::SocketBufferedPriv

#ifdef _WIN32
# include <WinSock2.h> // for fd_set
#else
# include <sys/select.h> // for fd_set
using SOCKET = int;
#endif // _WIN32

#include <atomic> // for std::atomic
#include <future> // for std::future
#include <mutex> // for std::mutex
#include <queue> // for std::queue
#include <vector> // for std::vector

struct SocketDriver::SocketDriverPriv
{
  using SocketRef = std::reference_wrapper<SocketAsync::SocketAsyncPriv>;

  // StepGuard and StopGuard perform a handshake to obtain stepMtx
  // with pauseMtx used to force Step() to yield

  struct StepGuard
  {
    std::unique_lock<std::recursive_mutex> stepLock;
    std::unique_lock<std::recursive_mutex> pauseLock;

    StepGuard(SocketDriverPriv &priv);
    StepGuard(StepGuard const &) = delete;
    StepGuard(StepGuard &&) = delete;
    ~StepGuard();
  };

  struct PauseGuard
  {
    std::unique_lock<std::recursive_mutex> stepLock;
    std::unique_lock<std::recursive_mutex> pauseLock;

    PauseGuard(SocketDriverPriv &priv);
    PauseGuard(PauseGuard const &) = delete;
    PauseGuard(PauseGuard &&) = delete;
    ~PauseGuard();
  };

  std::shared_ptr<SocketAddress::SocketAddressPriv> pipeToAddr;
  Socket::SocketPriv pipeFrom;
  Socket::SocketPriv pipeTo;
  std::recursive_mutex stepMtx;
  std::recursive_mutex pauseMtx;
  std::vector<SocketRef> sockets; // guarded by stepMtx
  std::atomic<bool> shouldStop;

  SocketDriverPriv();
  ~SocketDriverPriv();

  void Step(SocketBuffered::Time timeout);
  void Step(timeval *tv);

  void Run();
  void Stop();

  void Register(SocketAsync::SocketAsyncPriv &sock);
  void Unregister(SocketAsync::SocketAsyncPriv &sock);

  void Bump();
  void Unbump();

  std::tuple<SOCKET, fd_set, fd_set> PrepareFds();
  void DoOneFdTask(fd_set const &rfds,
                   fd_set const &wfds);
};

struct SocketAsync::SocketAsyncPriv : public SocketBuffered::SocketBufferedPriv
{
  using SendQElement = std::tuple<std::promise<void>, SocketBufferPtr>;
  using SendQ = std::queue<SendQElement>;
  using SendToQElement = std::tuple<std::promise<void>, SocketBufferPtr, SockAddr>;
  using SendToQ = std::queue<SendToQElement>;

  std::weak_ptr<SocketDriver::SocketDriverPriv> driver;
  Handlers handlers;
  std::mutex sendQMtx;
  SendQ sendQ;
  SendToQ sendToQ;
  std::shared_ptr<SocketAddress::SocketAddressPriv> peerAddr;

  SocketAsyncPriv(SocketPriv &&sock,
                  std::shared_ptr<SocketDriver::SocketDriverPriv> &driver,
                  Handlers handlers);
  SocketAsyncPriv(SocketBufferedPriv &&buff,
                  std::shared_ptr<SocketDriver::SocketDriverPriv> &driver,
                  Handlers handlers);
  virtual ~SocketAsyncPriv();

  std::future<void> Send(SocketBufferPtr &&buffer);
  std::future<void> SendTo(SocketBufferPtr &&buffer,
                           SockAddr const &dstAddr);
  template<typename QueueElement, typename... Args>
  std::future<void> DoSend(std::queue<QueueElement> &q,
                           Args&&... args);

  void DriverPrepareFds(SOCKET &fdMax,
                        fd_set &rfds,
                        fd_set &wfds);
  bool DriverDoFdTask(fd_set const &rfds,
                      fd_set const &wfds);
  void DriverDoFdTaskReadable();
  void DriverDoFdTaskWritable();
};

#endif // SOCKET_ASYNC_PRIV_H
