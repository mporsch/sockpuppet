#include "sockpuppet/socket_async.h"
#include "driver_impl.h" // for DriverImpl
#include "socket_async_impl.h" // for SocketAsyncImpl
#include "todo_impl.h" // for ToDoImpl

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
  : impl(std::make_shared<DriverImpl>())
{
}

void Driver::Step(Duration timeout)
{
  impl->Step(timeout);
}

void Driver::Run()
{
  impl->Run();
}

void Driver::Stop()
{
  impl->Stop();
}

Driver::Driver(Driver &&other) noexcept = default;

Driver::~Driver() = default;

Driver &Driver::operator=(Driver &&other) noexcept = default;


ToDo::ToDo(Driver &driver, std::function<void()> task)
  : impl(std::make_shared<ToDoImpl>(driver.impl, std::move(task)))
{
}

ToDo::ToDo(Driver &driver, std::function<void()> task, TimePoint when)
  : impl(std::make_shared<ToDoImpl>(driver.impl, std::move(task), when))
{
  driver.impl->ToDoInsert(impl);
}

ToDo::ToDo(Driver &driver, std::function<void()> task, Duration delay)
  : impl(std::make_shared<ToDoImpl>(driver.impl, std::move(task), Clock::now() + delay))
{
  driver.impl->ToDoInsert(impl);
}

void ToDo::Cancel()
{
  impl->Cancel();
}

void ToDo::Shift(TimePoint when)
{
  impl->Shift(when);
}

void ToDo::Shift(Duration delay)
{
  impl->Shift(Clock::now() + delay);
}

ToDo::ToDo(ToDo &&other) noexcept = default;

ToDo::~ToDo() = default;

ToDo &ToDo::operator=(ToDo &&other) noexcept = default;


SocketUdpAsync::SocketUdpAsync(SocketUdpBuffered &&buff,
    Driver &driver, ReceiveFromHandler handleReceiveFrom)
  : impl(std::make_unique<SocketAsyncImpl>(
      std::move(buff.impl),
      driver.impl,
      SocketAsyncImpl::Handlers{
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
  return impl->SendTo(std::move(buffer), dstAddress.impl);
}

Address SocketUdpAsync::LocalAddress() const
{
  return Address(impl->buff->sock->GetSockName());
}

SocketUdpAsync::SocketUdpAsync(SocketUdpAsync &&other) noexcept = default;

SocketUdpAsync::~SocketUdpAsync() = default;

SocketUdpAsync &SocketUdpAsync::operator=(SocketUdpAsync &&other) noexcept = default;


SocketTcpAsync::SocketTcpAsync(SocketTcpBuffered &&buff, Driver &driver,
    ReceiveHandler handleReceive, DisconnectHandler handleDisconnect)
  : impl(std::make_unique<SocketAsyncImpl>(
      std::move(buff.impl),
      driver.impl,
      SocketAsyncImpl::Handlers{
        std::move(checked(handleReceive))
      , nullptr
      , nullptr
      , std::move(checked(handleDisconnect))
      }))
{
}

std::future<void> SocketTcpAsync::Send(BufferPtr &&buffer)
{
  return impl->Send(std::move(buffer));
}

Address SocketTcpAsync::LocalAddress() const
{
  return Address(impl->buff->sock->GetSockName());
}

Address SocketTcpAsync::PeerAddress() const
{
  return Address(impl->buff->sock->GetPeerName());
}

SocketTcpAsync::SocketTcpAsync(SocketTcpAsync &&other) noexcept = default;

SocketTcpAsync::~SocketTcpAsync() = default;

SocketTcpAsync &SocketTcpAsync::operator=(SocketTcpAsync &&other) noexcept = default;


AcceptorAsync::AcceptorAsync(Acceptor &&sock, Driver &driver, ConnectHandler handleConnect)
  : impl(std::make_unique<SocketAsyncImpl>(
      std::move(sock.impl),
      driver.impl,
      SocketAsyncImpl::Handlers{
        nullptr
      , nullptr
      , std::move(checked(handleConnect))
      , nullptr
      }))
{
  impl->buff->sock->Listen();
}

Address AcceptorAsync::LocalAddress() const
{
  return Address(impl->buff->sock->GetSockName());
}

AcceptorAsync::AcceptorAsync(AcceptorAsync &&other) noexcept = default;

AcceptorAsync::~AcceptorAsync() = default;

AcceptorAsync &AcceptorAsync::operator=(AcceptorAsync &&other) noexcept = default;

} // namespace sockpuppet
