#ifndef SOCKET_ASYNC_PRIV
#define SOCKET_ASYNC_PRIV

#include "socket_address_priv.h" // for SocketAddress::SocketAddressPriv
#include "socket_async.h" // for SocketAsync
#include "socket_buffered_priv.h" // for SocketBuffered::SocketBufferedPriv

#ifdef _WIN32
# include <WinSock2.h> // for fd_set
#else
# include <sys/select.h> // for fd_set
using SOCKET = int;
#endif // _WIN32

#include <functional> // for std::function
#include <future> // for std::future
#include <mutex> // for std::mutex
#include <queue> // for std::queue
#include <vector> // for std::vector

struct SocketDriver::SocketDriverPriv
{
  using SocketRef = std::reference_wrapper<SocketAsync::SocketAsyncPriv>;
  using FdTask = std::function<void()>;

  std::vector<SocketRef> sockets;
  std::mutex socketsMtx;
  std::shared_ptr<SocketAddress::SocketAddressPriv> pipeToAddr;
  Socket::SocketPriv pipeFrom;
  Socket::SocketPriv pipeTo;
  bool shouldStop;

  SocketDriverPriv();
  ~SocketDriverPriv();

  void Step();

  void Run();
  void Stop();

  void Register(SocketAsync::SocketAsyncPriv &sock);
  void Unregister(SocketAsync::SocketAsyncPriv &sock);

  void Bump();
  void Unbump();

  std::tuple<SOCKET, fd_set, fd_set> PrepareFds();
  FdTask CollectFdTask(fd_set const &rfds, fd_set const &wfds);
};

struct SocketAsync::SocketAsyncPriv : public SocketBuffered::SocketBufferedPriv
{
  struct SendQElement
  {
    std::promise<void> promise;
    SocketBufferPtr buffer;
  };
  using SendQ = std::queue<SendQElement>;

  struct SendToQElement
  {
    std::promise<void> promise;
    SocketBufferPtr buffer;
    SockAddr addr;
  };
  using SendToQ = std::queue<SendToQElement>;

  std::weak_ptr<SocketDriver::SocketDriverPriv> driver;
  Handlers handlers;
  SendQ sendQ;
  SendToQ sendToQ;
  std::mutex sendQMtx;

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
                           Args&&... args)
  {
    std::promise<void> promise;
    auto ret = promise.get_future();

    {
      std::lock_guard<std::mutex> lock(sendQMtx);

      q.emplace(
        QueueElement{
          std::move(promise)
        , std::forward<Args>(args)...
        });
    }

    if(auto const ptr = driver.lock()) {
      ptr->Bump();
    }

    return ret;
  }

  void DriverPrepareFds(SOCKET &fdMax,
                        fd_set &rfds,
                        fd_set &wfds);

  SocketDriver::SocketDriverPriv::FdTask
  DriverCollectFdTask(fd_set const &rfds,
                      fd_set const &wfds);

  void DriverHandleReadable();
  void DriverHandleWritable();
};

#endif // SOCKET_ASYNC_PRIV
