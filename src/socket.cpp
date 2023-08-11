#include "sockpuppet/socket.h"
#include "address_impl.h" // for Address::AddressImpl
#include "socket_impl.h" // for SocketImpl
#include "socket_tls_impl.h" // for SocketTlsImpl

#ifdef _WIN32
# include <winsock2.h> // for IPPROTO_UDP
#else
# include <arpa/inet.h> // for IPPROTO_UDP
#endif // _WIN32

namespace sockpuppet {

SocketUdp::SocketUdp(Address const &bindAddress)
  : impl(std::make_unique<SocketImpl>(
      bindAddress.impl->Family(), SOCK_DGRAM, IPPROTO_UDP))
{
  impl->Bind(bindAddress.impl->ForUdp());
  impl->SetSockOptBroadcast();
  impl->SetSockOptNonBlocking();
}

size_t SocketUdp::SendTo(char const *data, size_t size,
    Address const &dstAddress, Duration timeout)
{
  return impl->SendTo(data, size, dstAddress.impl->ForUdp(), timeout);
}

std::optional<std::pair<size_t, Address>>
SocketUdp::ReceiveFrom(char *data, size_t size, Duration timeout)
{
  return impl->ReceiveFrom(data, size, timeout);
}

Address SocketUdp::LocalAddress() const
{
  return Address(impl->GetSockName());
}

size_t SocketUdp::ReceiveBufferSize() const
{
  return impl->GetSockOptRcvBuf();
}

SocketUdp::SocketUdp(SocketUdp &&other) noexcept = default;

SocketUdp::~SocketUdp() = default;

SocketUdp &SocketUdp::operator=(SocketUdp &&other) noexcept = default;


SocketTcp::SocketTcp(Address const &connectAddress)
  : impl(std::make_unique<SocketImpl>(
      connectAddress.impl->Family(), SOCK_STREAM, IPPROTO_TCP))
{
  impl->SetSockOptNoSigPipe();
  impl->Connect(connectAddress.impl->ForTcp());
  impl->SetSockOptNonBlocking();
}

#ifdef SOCKPUPPET_WITH_TLS
SocketTcp::SocketTcp(Address const &connectAddress,
    char const *certFilePath, char const *keyFilePath)
  : impl(std::make_unique<SocketTlsImpl>(
      connectAddress.impl->Family(), SOCK_STREAM, IPPROTO_TCP,
      certFilePath, keyFilePath))
{
  impl->SetSockOptNoSigPipe();
  impl->Connect(connectAddress.impl->ForTcp());
  impl->SetSockOptNonBlocking();
}
#endif // SOCKPUPPET_WITH_TLS

size_t SocketTcp::Send(char const *data, size_t size, Duration timeout)
{
  return impl->Send(data, size, timeout);
}

std::optional<size_t> SocketTcp::Receive(char *data, size_t size, Duration timeout)
{
  return impl->Receive(data, size, timeout);
}

Address SocketTcp::LocalAddress() const
{
  return Address(impl->GetSockName());
}

Address SocketTcp::PeerAddress() const
{
  return Address(impl->GetPeerName());
}

size_t SocketTcp::ReceiveBufferSize() const
{
  return impl->GetSockOptRcvBuf();
}

SocketTcp::SocketTcp(std::unique_ptr<SocketImpl> &&other)
  : impl(std::move(other))
{
  impl->SetSockOptNoSigPipe();
  impl->SetSockOptNonBlocking();
}

SocketTcp::SocketTcp(SocketTcp &&other) noexcept = default;

SocketTcp::~SocketTcp() = default;

SocketTcp &SocketTcp::operator=(SocketTcp &&other) noexcept = default;


Acceptor::Acceptor(Address const &bindAddress)
  : impl(std::make_unique<SocketImpl>(
      bindAddress.impl->Family(), SOCK_STREAM, IPPROTO_TCP))
{
  impl->SetSockOptReuseAddr();
  impl->Bind(bindAddress.impl->ForTcp());
}

#ifdef SOCKPUPPET_WITH_TLS
Acceptor::Acceptor(Address const &bindAddress,
    char const *certFilePath, char const *keyFilePath)
  : impl(std::make_unique<AcceptorTlsImpl>(
      bindAddress.impl->Family(), SOCK_STREAM, IPPROTO_TCP,
      certFilePath, keyFilePath))
{
  impl->SetSockOptReuseAddr();
  impl->Bind(bindAddress.impl->ForTcp());
  impl->SetSockOptNonBlocking();
}
#endif // SOCKPUPPET_WITH_TLS

std::optional<std::pair<SocketTcp, Address>>
Acceptor::Listen(Duration timeout)
{
  impl->Listen();
  return impl->Accept(timeout);
}

Address Acceptor::LocalAddress() const
{
  return Address(impl->GetSockName());
}

Acceptor::Acceptor(Acceptor &&other) noexcept = default;

Acceptor::~Acceptor() = default;

Acceptor &Acceptor::operator=(Acceptor &&other) noexcept = default;

} // namespace sockpuppet
