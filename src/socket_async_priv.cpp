#include "socket_async_priv.h"

#ifdef _WIN32
# include <io.h> // for ::pipe
# include <fcntl.h> // for O_BINARY
#else
# include <unistd.h> // for ::pipe
#endif // _WIN32

#include <cstring> // for std::strerror
#include <stdexcept> // for std::runtime_error
#include <string> // for std::string

namespace
{
  static size_t const pipeSize = 256U;
  int CreatePipe(int fdPipe[2])
  {
#ifdef _WIN32
    return ::_pipe(fdPipe, pipeSize, O_BINARY);
#else
    return ::pipe(fdPipe);
#endif // _WIN32
  }
} // unnamed namespace

SocketDriver::SocketDriverPriv::SocketDriverPriv()
  : shouldStop(false)
{
  if(CreatePipe(fdPipe)) {
    throw std::runtime_error("failed to create signalling pipe :"
                             + std::string(std::strerror(errno)));
  }
}

SocketDriver::SocketDriverPriv::~SocketDriverPriv()
{
  Stop();

  (void)::close(fdPipe[0]);
  (void)::close(fdPipe[1]);
}

void SocketDriver::SocketDriverPriv::Step()
{
  decltype(sockets) socks;
  {
    std::lock_guard<std::mutex> lock(socketsMtx);

    socks = sockets;
  }

  int fdMax = -1;
  fd_set rfds;
  fd_set wfds;
  FD_ZERO(&rfds);
  FD_ZERO(&wfds);
  for(auto &&sock : socks) {
    sock.get().AsyncFillFdSet(fdMax, rfds, wfds);
  }

  FD_SET(fdPipe[0], &rfds);
  fdMax = std::max(fdMax, fdPipe[0]);

  if(auto const result = ::select(fdMax + 1, &rfds, &wfds, nullptr, nullptr)) {
    if(result < 0) {
      throw std::runtime_error("select failed: "
                               + std::string(std::strerror(errno)));
    } else {
      // readable/writable
      if(FD_ISSET(fdPipe[0], &rfds)) {
        char dump[pipeSize];
        (void)::read(fdPipe[0], dump, sizeof(dump));
      } else {
        for(auto &&sock : socks) {
          sock.get().AsyncCheckFdSet(rfds, wfds);
        }
      }
    }
  } else {
    throw std::runtime_error("select timed out");
  }
}

void SocketDriver::SocketDriverPriv::Run()
{
  while(!shouldStop) {
    Step();
  }
  shouldStop = false;
}

void SocketDriver::SocketDriverPriv::Stop()
{
  shouldStop = true;
  Bump();
}

void SocketDriver::SocketDriverPriv::Register(
    SocketAsync::SocketAsyncPriv &sock)
{
  std::lock_guard<std::mutex> lock(socketsMtx);

  sockets.emplace_back(sock);
}

void SocketDriver::SocketDriverPriv::Unregister(
    SocketAsync::SocketAsyncPriv &sock)
{
  {
    std::lock_guard<std::mutex> lock(socketsMtx);

    auto cmp = [&](std::reference_wrapper<SocketAsync::SocketAsyncPriv> s) -> bool
    {
      return (&s.get() == &sock);
    };

    sockets.erase(
          std::remove_if(std::begin(sockets),
                         std::end(sockets),
                         cmp),
          std::end(sockets));
  }

  Bump();
}

void SocketDriver::SocketDriverPriv::Bump()
{
  static char const one = '1';
  auto const result = ::write(fdPipe[1], &one, sizeof(one));
  if(result < 0) {
    throw std::runtime_error("failed write to signalling pipe :"
                             + std::string(std::strerror(errno)));
  }
}


SocketAsync::SocketAsyncPriv::SocketAsyncPriv(
    Socket::SocketPriv &&sock,
    std::shared_ptr<SocketDriver::SocketDriverPriv> &driver,
    SocketAsync::Handlers handlers)
  : SocketAsyncPriv(SocketBufferedPriv(std::move(sock), 0U, 0U),
                    driver,
                    handlers)
{
}

SocketAsync::SocketAsyncPriv::SocketAsyncPriv(
    SocketBuffered::SocketBufferedPriv &&buff,
    std::shared_ptr<SocketDriver::SocketDriverPriv> &driver,
    SocketAsync::Handlers handlers)
  : SocketBuffered::SocketBufferedPriv(std::move(buff))
  , driver(driver)
  , handlers(handlers)
{
  driver->Register(*this);
}

SocketAsync::SocketAsyncPriv::~SocketAsyncPriv()
{
  if(auto const ptr = driver.lock()) {
    ptr->Unregister(*this);
  }
}

std::future<void> SocketAsync::SocketAsyncPriv::Send(
  SocketBufferPtr &&buffer)
{
  std::promise<void> promise;
  auto ret = promise.get_future();

  {
    std::lock_guard<std::mutex> lock(sendQMtx);

    sendQ.emplace(SendQElement{
      std::move(promise)
    , std::move(buffer)
    });
  }

  if(auto const ptr = driver.lock()) {
    ptr->Bump();
  }

  return ret;
}

std::future<void> SocketAsync::SocketAsyncPriv::SendTo(
  SocketBufferPtr &&buffer, SockAddr const &dstAddr)
{
  std::promise<void> promise;
  auto ret = promise.get_future();

  {
    std::lock_guard<std::mutex> lock(sendQMtx);

    sendToQ.emplace(SendToQElement{
      std::move(promise)
    , std::move(buffer)
    , dstAddr
    });
  }

  if(auto const ptr = driver.lock()) {
    ptr->Bump();
  }

  return ret;
}

void SocketAsync::SocketAsyncPriv::AsyncFillFdSet(int &fdMax,
  fd_set &rfds, fd_set &wfds)
{
  if(handlers.connect || handlers.receive || handlers.receiveFrom) {
    fdMax = std::max(fdMax, static_cast<int>(this->fd));
    FD_SET(this->fd, &rfds);
  }

  {
    std::lock_guard<std::mutex> lock(sendQMtx);

    if(!sendQ.empty() || !sendToQ.empty()) {
      fdMax = std::max(fdMax, static_cast<int>(this->fd));
      FD_SET(this->fd, &wfds);
    }
  }
}

void SocketAsync::SocketAsyncPriv::AsyncCheckFdSet(fd_set const &rfds,
  fd_set const &wfds)
{
  try {
    if(FD_ISSET(this->fd, &rfds)) {
      if(handlers.connect) {
        auto t = this->Accept(Socket::Time(0));

        handlers.connect(
              std::tuple<SocketTcpClient, SocketAddress>{
                SocketTcpClient(std::move(std::get<0>(t)))
              , SocketAddress(std::move(std::get<1>(t)))
              });

        this->Listen();
      } else if(handlers.receive) {
        handlers.receive(this->Receive(Socket::Time(0)));
      } else if(handlers.receiveFrom) {
        handlers.receiveFrom(this->ReceiveFrom(Socket::Time(0)));
      }
    }

    if(FD_ISSET(this->fd, &wfds)) {
      std::unique_lock<std::mutex> lock(sendQMtx);

      if(!sendQ.empty()) {
        auto e = std::move(sendQ.front());
        sendQ.pop();
        lock.unlock();

        SocketPriv::Send(e.buffer->data(), e.buffer->size(), Socket::Time(0));
        e.promise.set_value();
      } else if(!sendToQ.empty()) {
        auto e = std::move(sendToQ.front());
        sendToQ.pop();
        lock.unlock();

        SocketPriv::SendTo(e.buffer->data(), e.buffer->size(), e.addr);
        e.promise.set_value();
      }
    }
  } catch(std::exception const &) {
    if(auto const ptr = driver.lock()) {
      ptr->Unregister(*this);
    }

    if(handlers.disconnect) {
      handlers.disconnect(nullptr); // TODO
    }
  }
}
