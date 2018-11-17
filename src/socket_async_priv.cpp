#include "socket_async_priv.h"

#include <cstring> // for std::strerror
#include <stdexcept> // for std::runtime_error
#include <string> // for std::string

SocketDriver::SocketDriverPriv::SocketDriverPriv()
  : shouldStop(false)
  , pipeToAddr(SocketAddressAddrinfo("localhost:65369"))
  , pipeFrom(pipeToAddr.SockAddrUdp().family, SOCK_DGRAM, IPPROTO_UDP)
  , pipeTo(pipeToAddr.SockAddrUdp().family, SOCK_DGRAM, IPPROTO_UDP)
{
  pipeTo.Bind(pipeToAddr.SockAddrUdp());

  SocketAddressAddrinfo pipeFromAddr(0);
  pipeFrom.Bind(pipeFromAddr.SockAddrUdp());
}

SocketDriver::SocketDriverPriv::~SocketDriverPriv()
{
  Stop();
}

void SocketDriver::SocketDriverPriv::Step()
{
  auto t = PrepareFds();
  auto &&fdMax = std::get<0>(t);
  auto &&rfds = std::get<1>(t);
  auto &&wfds = std::get<2>(t);

  if(auto const result = ::select(static_cast<int>(fdMax + 1), &rfds, &wfds, nullptr, nullptr)) {
    if(result < 0) {
      throw std::runtime_error("select failed: "
                               + std::string(std::strerror(errno)));
    }
  } else {
    throw std::runtime_error("select timed out");
  }

  // one or more sockets is readable/writable
  if(FD_ISSET(pipeTo.fd, &rfds)) {
    // a readable signalling socket triggers re-evaluating the sockets
    Unbump();
  } else if(auto task = CollectFdTask(rfds, wfds)) {
    // because any of the user tasks may unregister/destroy a socket,
    // the safe approach is to handle only one task at a time
    task();
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

    sockets.erase(
      std::remove_if(
        std::begin(sockets),
        std::end(sockets),
        [&](SocketRef s) -> bool
        {
          return (&s.get() == &sock);
        }),
      std::end(sockets));
  }

  Bump();
}

void SocketDriver::SocketDriverPriv::Bump()
{
  static char const one = '1';
  pipeFrom.SendTo(&one, sizeof(one), pipeToAddr.SockAddrUdp());
}

void SocketDriver::SocketDriverPriv::Unbump()
{
  char dump[256U];
  (void)pipeTo.Receive(dump, sizeof(dump), Socket::Time(0));
}

std::tuple<SOCKET, fd_set, fd_set> SocketDriver::SocketDriverPriv::PrepareFds()
{
  std::lock_guard<std::mutex> lock(socketsMtx);

  SOCKET fdMax = -1;
  fd_set rfds;
  fd_set wfds;
  FD_ZERO(&rfds);
  FD_ZERO(&wfds);

  // have the sockets set their file descriptors
  for(auto &&sock : sockets) {
    sock.get().DriverPrepareFds(fdMax, rfds, wfds);
  }

  // set the signalling socket file descriptor
  FD_SET(pipeTo.fd, &rfds);
  fdMax = std::max(fdMax, pipeTo.fd);

  return std::tuple<SOCKET, fd_set, fd_set>{
    fdMax
  , rfds
  , wfds
  };
}

std::function<void()> SocketDriver::SocketDriverPriv::CollectFdTask(
  fd_set const &rfds, fd_set const &wfds)
{
  std::lock_guard<std::mutex> lock(socketsMtx);

  for(auto &&sock : sockets) {
    if(auto const task = sock.get().DriverCollectFdTask(rfds, wfds)) {
      return task;
    }
  }

  return nullptr;
}


SocketAsync::SocketAsyncPriv::SocketAsyncPriv(
    SocketPriv &&sock,
    std::shared_ptr<SocketDriver::SocketDriverPriv> &driver,
    Handlers handlers)
  : SocketAsyncPriv(SocketBufferedPriv(std::move(sock), 0U, 0U),
                    driver,
                    handlers)
{
}

SocketAsync::SocketAsyncPriv::SocketAsyncPriv(
    SocketBufferedPriv &&buff,
    std::shared_ptr<SocketDriver::SocketDriverPriv> &driver,
    Handlers handlers)
  : SocketBufferedPriv(std::move(buff))
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
  return DoSend(sendQ,
                std::move(buffer));
}

std::future<void> SocketAsync::SocketAsyncPriv::SendTo(
  SocketBufferPtr &&buffer, SockAddr const &dstAddr)
{
  return DoSend(sendToQ,
                std::move(buffer),
                dstAddr);
}

void SocketAsync::SocketAsyncPriv::DriverPrepareFds(SOCKET &fdMax,
  fd_set &rfds, fd_set &wfds)
{
  if(handlers.connect || handlers.receive || handlers.receiveFrom) {
    fdMax = std::max(fdMax, this->fd);
    FD_SET(this->fd, &rfds);
  }

  {
    std::lock_guard<std::mutex> lock(sendQMtx);

    if(!sendQ.empty() || !sendToQ.empty()) {
      fdMax = std::max(fdMax, this->fd);
      FD_SET(this->fd, &wfds);
    }
  }
}

std::function<void()> SocketAsync::SocketAsyncPriv::DriverCollectFdTask(
  fd_set const &rfds, fd_set const &wfds)
{
  if(FD_ISSET(this->fd, &rfds)) {
    return std::bind(&SocketAsyncPriv::DriverHandleReadable, this);
  } else if(FD_ISSET(this->fd, &wfds)) {
    return std::bind(&SocketAsyncPriv::DriverHandleWritable, this);
  } else {
    return nullptr;
  }
}

void SocketAsync::SocketAsyncPriv::DriverHandleReadable()
try {
  if(handlers.connect) {
    auto t = this->Accept(Time(0));
    this->Listen();

    handlers.connect(
      std::tuple<SocketTcpClient, SocketAddress>{
        SocketTcpClient(std::move(std::get<0>(t)))
      , SocketAddress(std::move(std::get<1>(t)))
      });
  } else if(handlers.receive) {
    handlers.receive(this->Receive(Time(0)));
  } else if(handlers.receiveFrom) {
    handlers.receiveFrom(this->ReceiveFrom(Time(0)));
  } else {
    throw std::logic_error("no read handler");
  }
} catch(std::runtime_error const &) {
  if(handlers.disconnect) {
    handlers.disconnect(nullptr); // TODO
  }
}

void SocketAsync::SocketAsyncPriv::DriverHandleWritable()
{
  std::unique_lock<std::mutex> lock(sendQMtx);

  if(!sendQ.empty()) {
    auto e = std::move(sendQ.front());
    sendQ.pop();
    lock.unlock();

    SocketPriv::Send(e.buffer->data(), e.buffer->size(), Time(0));
    e.promise.set_value();
  } else if(!sendToQ.empty()) {
    auto e = std::move(sendToQ.front());
    sendToQ.pop();
    lock.unlock();

    SocketPriv::SendTo(e.buffer->data(), e.buffer->size(), e.addr);
    e.promise.set_value();
  } else {
    throw std::logic_error("send buffer emptied unexpectedly");
  }
}
