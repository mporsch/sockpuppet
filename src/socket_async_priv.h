#ifndef SOCKET_ASYNC_PRIV
#define SOCKET_ASYNC_PRIV

#include "socket_address_priv.h" // for SocketAddress::SocketAddressPriv
#include "socket_async.h" // for SocketAsync
#include "socket_buffered_priv.h" // for SocketBuffered::SocketBufferedPriv

#ifdef _WIN32
# include <Winsock2.h> // for SOCKET
#else
# include <sys/select.h> // for fd_set
using SOCKET = int;
#endif // _WIN32

#include <future> // for std::future
#include <mutex> // for std::mutex
#include <queue> // for std::queue

struct SocketDriver::SocketDriverPriv
{
  std::vector<std::reference_wrapper<SocketAsync::SocketAsyncPriv>> sockets;
  std::mutex socketsMtx;
  int fdPipe[2];
  bool shouldStop;

  SocketDriverPriv();
  ~SocketDriverPriv();

  void Step();

  void Run();
  void Stop();

  void Register(SocketAsync::SocketAsyncPriv &sock);
  void Unregister(SocketAsync::SocketAsyncPriv &sock);

  void Bump();
};

struct SocketAsync::SocketAsyncPriv : public SocketBuffered::SocketBufferedPriv
{
  struct SendQElement
  {
    std::promise<void> promise;
    SocketBuffered::SocketBufferPtr buffer;
  };
  using SendQ = std::queue<SendQElement>;

  struct SendToQElement
  {
    std::promise<void> promise;
    SocketBuffered::SocketBufferPtr buffer;
    SockAddr addr;
  };
  using SendToQ = std::queue<SendToQElement>;

  std::weak_ptr<SocketDriver::SocketDriverPriv> driver;
  SocketAsync::Handlers handlers;
  SendQ sendQ;
  SendToQ sendToQ;
  std::mutex sendQMtx;

  SocketAsyncPriv(Socket::SocketPriv &&sock,
                  std::shared_ptr<SocketDriver::SocketDriverPriv> &driver,
                  SocketAsync::Handlers handlers);
  SocketAsyncPriv(SocketBuffered::SocketBufferedPriv &&buff,
                  std::shared_ptr<SocketDriver::SocketDriverPriv> &driver,
                  SocketAsync::Handlers handlers);
  ~SocketAsyncPriv();

  std::future<void> Send(SocketBuffered::SocketBufferPtr buffer);

  std::future<void> SendTo(SocketBuffered::SocketBufferPtr buffer,
                           SockAddr const &dstAddr);

  void AsyncFillFdSet(SOCKET &fdMax,
                      fd_set &rfds,
                      fd_set &wfds);
  void AsyncCheckFdSet(fd_set const &rfds,
                       fd_set const &wfds);
  void AsyncSend();
  void AsyncReceive();
};

#endif // SOCKET_ASYNC_PRIV
