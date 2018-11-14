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
  decltype(sockets) socks;
  {
    std::lock_guard<std::mutex> lock(socketsMtx);

    socks = sockets;
  }

  SOCKET fdMax = -1;
  fd_set rfds;
  fd_set wfds;
  FD_ZERO(&rfds);
  FD_ZERO(&wfds);
  for(auto &&sock : socks) {
    sock.get().AsyncFillFdSet(fdMax, rfds, wfds);
  }

  FD_SET(pipeTo.fd, &rfds);
  fdMax = std::max(fdMax, pipeTo.fd);

  if(auto const result = ::select(static_cast<int>(fdMax + 1), &rfds, &wfds, nullptr, nullptr)) {
    if(result < 0) {
      throw std::runtime_error("select failed: "
                               + std::string(std::strerror(errno)));
    } else {
      // readable/writable
      if(FD_ISSET(pipeTo.fd, &rfds)) {
        char dump[256U];
        (void)pipeTo.Receive(dump, sizeof(dump), Socket::Time(0));
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
  pipeFrom.SendTo(&one, sizeof(one), pipeToAddr.SockAddrUdp());
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

void SocketAsync::SocketAsyncPriv::AsyncFillFdSet(SOCKET &fdMax,
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
