#include "sockpuppet/socket_async.h"
#include "driver_priv.h" // for DriverPriv
#include "socket_async_priv.h" // for SocketAsyncPriv
#include "todo_priv.h" // for ToDoPriv

#include <stdexcept> // for std::logic_error

namespace sockpuppet {

namespace {
  template<typename Fn>
  std::function<Fn> &checked(std::function<Fn> &handler)
  {
    if(!handler) {
      throw std::logic_error("invalid handler");
    }
    return handler;
  }
} // unnamed namespace

Driver::Driver()
  : priv(std::make_shared<DriverPriv>())
{
}

void Driver::Step(Duration timeout)
{
  priv->Step(timeout);
}

void Driver::Run()
{
  priv->Run();
}

void Driver::Stop()
{
  priv->Stop();
}

Driver::Driver(Driver &&other) noexcept = default;

Driver::~Driver() = default;

Driver &Driver::operator=(Driver &&other) noexcept = default;


ToDo::ToDo(Driver &driver, std::function<void()> task)
  : priv(std::make_shared<ToDoPriv>(driver.priv, std::move(task)))
{
}

ToDo::ToDo(Driver &driver, std::function<void()> task, TimePoint when)
  : priv(std::make_shared<ToDoPriv>(driver.priv, std::move(task), when))
{
  driver.priv->ToDoInsert(priv);
}

ToDo::ToDo(Driver &driver, std::function<void()> task, Duration delay)
  : priv(std::make_shared<ToDoPriv>(driver.priv, std::move(task), Clock::now() + delay))
{
  driver.priv->ToDoInsert(priv);
}

void ToDo::Cancel()
{
  priv->Cancel();
}

void ToDo::Shift(TimePoint when)
{
  priv->Shift(when);
}

void ToDo::Shift(Duration delay)
{
  priv->Shift(Clock::now() + delay);
}

ToDo::ToDo(ToDo &&other) noexcept = default;

ToDo::~ToDo() = default;

ToDo &ToDo::operator=(ToDo &&other) noexcept = default;


SocketUdpAsync::SocketUdpAsync(SocketUdpBuffered &&buff,
    Driver &driver, ReceiveFromHandler handleReceiveFrom)
  : priv(std::make_unique<SocketAsyncPriv>(
      std::move(*buff.priv),
      driver.priv,
      SocketAsyncPriv::Handlers{
        nullptr
      , std::move(checked(handleReceiveFrom))
      , nullptr
      , nullptr
      }))
{
}

std::future<void> SocketUdpAsync::SendTo(BufferPtr &&buffer,
    Address const &dstAddress)
{
  return priv->SendTo(std::move(buffer), dstAddress.priv);
}

Address SocketUdpAsync::LocalAddress() const
{
  return Address(priv->GetSockName());
}

SocketUdpAsync::SocketUdpAsync(SocketUdpAsync &&other) noexcept = default;

SocketUdpAsync::~SocketUdpAsync() = default;

SocketUdpAsync &SocketUdpAsync::operator=(SocketUdpAsync &&other) noexcept = default;


SocketTcpAsyncClient::SocketTcpAsyncClient(
    SocketTcpBuffered &&buff, Driver &driver,
    ReceiveHandler handleReceive, DisconnectHandler handleDisconnect)
  : priv(std::make_unique<SocketAsyncPriv>(
      std::move(*buff.priv),
      driver.priv,
      SocketAsyncPriv::Handlers{
        std::move(checked(handleReceive))
      , nullptr
      , nullptr
      , std::move(checked(handleDisconnect))
      }))
{
}

std::future<void> SocketTcpAsyncClient::Send(BufferPtr &&buffer)
{
  return priv->Send(std::move(buffer));
}

Address SocketTcpAsyncClient::LocalAddress() const
{
  return Address(priv->GetSockName());
}

Address SocketTcpAsyncClient::PeerAddress() const
{
  return Address(priv->GetPeerName());
}

SocketTcpAsyncClient::SocketTcpAsyncClient(SocketTcpAsyncClient &&other) noexcept = default;

SocketTcpAsyncClient::~SocketTcpAsyncClient() = default;

SocketTcpAsyncClient &SocketTcpAsyncClient::operator=(SocketTcpAsyncClient &&other) noexcept = default;


SocketTcpAsyncServer::SocketTcpAsyncServer(SocketTcpServer &&sock,
    Driver &driver, ConnectHandler handleConnect)
  : priv(std::make_unique<SocketAsyncPriv>(
      std::move(*sock.priv),
      driver.priv,
      SocketAsyncPriv::Handlers{
        nullptr
      , nullptr
      , std::move(checked(handleConnect))
      , nullptr
      }))
{
  priv->Listen();
}

Address SocketTcpAsyncServer::LocalAddress() const
{
  return Address(priv->GetSockName());
}

SocketTcpAsyncServer::SocketTcpAsyncServer(SocketTcpAsyncServer &&other) noexcept = default;

SocketTcpAsyncServer::~SocketTcpAsyncServer() = default;

SocketTcpAsyncServer &SocketTcpAsyncServer::operator=(SocketTcpAsyncServer &&other) noexcept = default;

} // namespace sockpuppet
