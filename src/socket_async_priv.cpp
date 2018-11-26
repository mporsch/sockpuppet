#include "socket_async_priv.h"

#include <cstring> // for std::strerror
#include <stdexcept> // for std::runtime_error
#include <string> // for std::string

SocketDriver::SocketDriverPriv::StepGuard::StepGuard(SocketDriverPriv &priv)
  : stepLock(priv.stepMtx)
  , pauseLock(priv.pauseMtx, std::defer_lock)
{
}

SocketDriver::SocketDriverPriv::StepGuard::~StepGuard()
{
  stepLock.unlock();
  pauseLock.lock();
}


SocketDriver::SocketDriverPriv::PauseGuard::PauseGuard(SocketDriverPriv &priv)
  : stepLock(priv.stepMtx, std::defer_lock)
  , pauseLock(priv.pauseMtx, std::defer_lock)
{
  if(!stepLock.try_lock()) {
    pauseLock.lock();
    priv.Bump();
    stepLock.lock();
  }
}

SocketDriver::SocketDriverPriv::PauseGuard::~PauseGuard()
{
}


SocketDriver::SocketDriverPriv::SocketDriverPriv()
  : pipeToAddr(std::make_shared<SocketAddressAddrinfo>(0))
  , pipeFrom(pipeToAddr->SockAddrUdp().family, SOCK_DGRAM, IPPROTO_UDP)
  , pipeTo(pipeToAddr->SockAddrUdp().family, SOCK_DGRAM, IPPROTO_UDP)
  , shouldStop(false)
{
  // bind to system-assigned port number and update address accordingly
  pipeTo.Bind(pipeToAddr->SockAddrUdp());
  pipeToAddr = pipeTo.GetSockName();

  SocketAddressAddrinfo pipeFromAddr(0);
  pipeFrom.Bind(pipeFromAddr.SockAddrUdp());
}

SocketDriver::SocketDriverPriv::~SocketDriverPriv()
{
  Stop();
}

void SocketDriver::SocketDriverPriv::Step(SocketBuffered::Time timeout)
{
  if(timeout.count() > 0U) {
    auto tv = Socket::SocketPriv::ToTimeval(timeout);
    Step(&tv);
  } else {
    Step(nullptr);
  }
}

void SocketDriver::SocketDriverPriv::Step(timeval *tv)
{
  StepGuard guard(*this);

  auto t = PrepareFds();
  auto &&fdMax = std::get<0>(t);
  auto &&rfds = std::get<1>(t);
  auto &&wfds = std::get<2>(t);

  if(auto const result = ::select(static_cast<int>(fdMax + 1),
                                  &rfds,
                                  &wfds,
                                  nullptr,
                                  tv)) {
    if(result < 0) {
      throw std::runtime_error("select failed: "
                               + std::string(std::strerror(errno)));
    }
  } else {
    return;
  }

  // one or more sockets is readable/writable
  if(FD_ISSET(pipeTo.fd, &rfds)) {
    // a readable signalling socket triggers re-evaluating the sockets
    Unbump();
  } else {
    DoOneFdTask(rfds, wfds);
  }
}

void SocketDriver::SocketDriverPriv::Run()
{
  while(!shouldStop) {
    Step(nullptr);
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
  PauseGuard guard(*this);
  sockets.emplace_back(sock);
}

void SocketDriver::SocketDriverPriv::Unregister(
    SocketAsync::SocketAsyncPriv &sock)
{
  PauseGuard guard(*this);
  sockets.erase(
    std::remove_if(
      std::begin(sockets),
      std::end(sockets),
      [&](SocketRef s) -> bool {
        return (&s.get() == &sock);
      }),
    std::end(sockets));
}

void SocketDriver::SocketDriverPriv::Bump()
{
  static char const one = '1';
  pipeFrom.SendTo(&one, sizeof(one), pipeToAddr->SockAddrUdp());
}

void SocketDriver::SocketDriverPriv::Unbump()
{
  char dump[256U];
  (void)pipeTo.Receive(dump, sizeof(dump), Socket::Time(0));
}

std::tuple<SOCKET, fd_set, fd_set>
SocketDriver::SocketDriverPriv::PrepareFds()
{
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

void SocketDriver::SocketDriverPriv::DoOneFdTask(
    fd_set const &rfds, fd_set const &wfds)
{
  // user task may unregister/destroy a socket -> handle only one
  for(auto &&sock : sockets) {
    if(sock.get().DriverDoFdTask(rfds, wfds)) {
      break;
    }
  }
}


SocketAsync::SocketAsyncPriv::SocketAsyncPriv(SocketPriv &&sock,
    std::shared_ptr<SocketDriver::SocketDriverPriv> &driver, Handlers handlers)
  : SocketAsyncPriv(SocketBufferedPriv(std::move(sock), 0U, 0U),
                    driver,
                    handlers)
{
}

SocketAsync::SocketAsyncPriv::SocketAsyncPriv(SocketBufferedPriv &&buff,
    std::shared_ptr<SocketDriver::SocketDriverPriv> &driver, Handlers handlers)
  : SocketBufferedPriv(std::move(buff))
  , driver(driver)
  , handlers(handlers)
{
  driver->Register(*this);

  if(handlers.disconnect) {
    peerAddr = this->GetPeerName();
  }
}

SocketAsync::SocketAsyncPriv::~SocketAsyncPriv()
{
  if(auto const ptr = driver.lock()) {
    ptr->Unregister(*this);
  }
}

std::future<void> SocketAsync::SocketAsyncPriv::Send(SocketBufferPtr &&buffer)
{
  return DoSend(sendQ, std::move(buffer));
}

std::future<void> SocketAsync::SocketAsyncPriv::SendTo(
    SocketBufferPtr &&buffer, SockAddr const &dstAddr)
{
  return DoSend(sendToQ, std::move(buffer), dstAddr);
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

bool SocketAsync::SocketAsyncPriv::DriverDoFdTask(
    fd_set const &rfds, fd_set const &wfds)
{
  if(FD_ISSET(this->fd, &rfds)) {
    DriverDoFdTaskReadable();
    return true;
  } else if(FD_ISSET(this->fd, &wfds)) {
    DriverDoFdTaskWritable();
    return true;
  }
  return false;
}

void SocketAsync::SocketAsyncPriv::DriverDoFdTaskReadable()
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
    handlers.disconnect(SocketAddress(peerAddr));
  }
}

void SocketAsync::SocketAsyncPriv::DriverDoFdTaskWritable()
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
