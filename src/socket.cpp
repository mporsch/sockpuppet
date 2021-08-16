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


SocketTcpClient::SocketTcpClient(Address const &connectAddress)
  : impl(std::make_unique<SocketImpl>(
      connectAddress.impl->Family(), SOCK_STREAM, IPPROTO_TCP))
{
  impl->SetSockOptNoSigPipe();
  impl->Connect(connectAddress.impl->ForTcp());
}

#ifdef SOCKPUPPET_WITH_TLS
SocketTcpClient::SocketTcpClient(Address const &connectAddress,
    char const *certFilePath, char const *keyFilePath)
  : impl(std::make_unique<SocketTlsClientImpl>(
      connectAddress.impl->Family(), SOCK_STREAM, IPPROTO_TCP,
      certFilePath, keyFilePath))
{
  impl->SetSockOptNoSigPipe();
  impl->Connect(connectAddress.impl->ForTcp());
}
#endif // SOCKPUPPET_WITH_TLS

size_t SocketTcpClient::Send(char const *data, size_t size, Duration timeout)
{
  return impl->Send(data, size, timeout);
}

std::optional<size_t> SocketTcpClient::Receive(char *data, size_t size, Duration timeout)
{
  return impl->Receive(data, size, timeout);
}

Address SocketTcpClient::LocalAddress() const
{
  return Address(impl->GetSockName());
}

Address SocketTcpClient::PeerAddress() const
{
  return Address(impl->GetPeerName());
}

size_t SocketTcpClient::ReceiveBufferSize() const
{
  return impl->GetSockOptRcvBuf();
}

SocketTcpClient::SocketTcpClient(std::unique_ptr<SocketImpl> &&other)
  : impl(std::move(other))
{
  impl->SetSockOptNoSigPipe();
}

SocketTcpClient::SocketTcpClient(SocketTcpClient &&other) noexcept = default;

SocketTcpClient::~SocketTcpClient() = default;

SocketTcpClient &SocketTcpClient::operator=(SocketTcpClient &&other) noexcept = default;


SocketTcpServer::SocketTcpServer(Address const &bindAddress)
  : impl(std::make_unique<SocketImpl>(
      bindAddress.impl->Family(), SOCK_STREAM, IPPROTO_TCP))
{
  impl->SetSockOptReuseAddr();
  impl->Bind(bindAddress.impl->ForTcp());
}

#ifdef SOCKPUPPET_WITH_TLS
SocketTcpServer::SocketTcpServer(Address const &bindAddress,
    char const *certFilePath, char const *keyFilePath)
  : impl(std::make_unique<SocketTlsServerImpl>(
      bindAddress.impl->Family(), SOCK_STREAM, IPPROTO_TCP,
      certFilePath, keyFilePath))
{
  impl->SetSockOptReuseAddr();
  impl->Bind(bindAddress.impl->ForTcp());
}
#endif // SOCKPUPPET_WITH_TLS

std::optional<std::pair<SocketTcpClient, Address>>
SocketTcpServer::Listen(Duration timeout)
{
  impl->Listen();
  return impl->Accept(timeout);
}

Address SocketTcpServer::LocalAddress() const
{
  return Address(impl->GetSockName());
}

SocketTcpServer::SocketTcpServer(SocketTcpServer &&other) noexcept = default;

SocketTcpServer::~SocketTcpServer() = default;

SocketTcpServer &SocketTcpServer::operator=(SocketTcpServer &&other) noexcept = default;

} // namespace sockpuppet
