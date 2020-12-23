#ifndef SOCKPUPPET_SOCKET_ASYNC_PRIV_H
#define SOCKPUPPET_SOCKET_ASYNC_PRIV_H

#include "address_priv.h" // for Address::AddressPriv
#include "sockpuppet/socket_async.h" // for SocketAsync
#include "socket_buffered_priv.h" // for SocketBuffered::SocketBufferedPriv

#ifdef _WIN32
# include <winsock2.h> // for pollfd
#else
# include <poll.h> // for pollfd
#endif // _WIN32

#include <atomic> // for std::atomic
#include <future> // for std::future
#include <mutex> // for std::mutex
#include <queue> // for std::queue
#include <tuple> // for std::tuple
#include <vector> // for std::vector

namespace sockpuppet {

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
  std::shared_ptr<Address::AddressPriv> pipeToAddr;
  SocketPriv pipeFrom;
  SocketPriv pipeTo;

  std::recursive_mutex stepMtx;
  std::mutex pauseMtx;
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

  void Run();
  void Stop();

  // interface for SocketAsyncPriv
  void AsyncRegister(SocketAsyncPriv &sock);
  void AsyncUnregister(SOCKET fd);
  void AsyncWantSend(SOCKET fd);

  void Bump();
  void Unbump();

  void DoOneFdTask();
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
  using SendToQElement = std::tuple<std::promise<void>, BufferPtr, Address>;
  using SendToQ = std::queue<SendToQElement>;

  std::weak_ptr<SocketDriver::SocketDriverPriv> driver;
  Handlers handlers;
  std::mutex sendQMtx;
  SendQ sendQ;
  SendToQ sendToQ;
  std::shared_ptr<Address::AddressPriv> peerAddr;

  SocketAsyncPriv(SocketPriv &&sock,
                  std::shared_ptr<SocketDriver::SocketDriverPriv> &driver,
                  Handlers handlers);
  SocketAsyncPriv(SocketBufferedPriv &&buff,
                  std::shared_ptr<SocketDriver::SocketDriverPriv> &driver,
                  Handlers handlers);
  SocketAsyncPriv(SocketAsyncPriv const &) = delete;
  SocketAsyncPriv(SocketAsyncPriv &&) = delete;
  ~SocketAsyncPriv() override;
  SocketAsyncPriv &operator=(SocketAsyncPriv const &) = delete;
  SocketAsyncPriv &operator=(SocketAsyncPriv &&) = delete;

  std::future<void> Send(BufferPtr &&buffer);
  std::future<void> SendTo(BufferPtr &&buffer,
                           Address const &dstAddr);
  template<typename QueueElement, typename... Args>
  std::future<void> DoSend(std::queue<QueueElement> &q,
                           Args&&... args);

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
