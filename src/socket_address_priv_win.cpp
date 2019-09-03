#ifdef _WIN32

#include "socket_address_priv.h"

#include <iphlpapi.h> // for ::GetAdaptersInfo

#include <stdexcept> // for std::runtime_error

namespace sockpuppet {

SocketAddress SocketAddress::SocketAddressPriv::ToBroadcast(uint16_t port) const
{
  auto const host = Host();

  // GetAdaptersInfo returns IPv4 addresses only
  auto getAdaptersInfo = []() -> std::unique_ptr<char const[]> {
    ULONG storageSize = 4096U;
    std::unique_ptr<char[]> storage(new char[storageSize]);

    for(int i = 0; i < 2; ++i) {
      if(::GetAdaptersInfo(
           reinterpret_cast<IP_ADAPTER_INFO *>(storage.get()),
           &storageSize)) {
        // may fail once on insufficient buffer size
        // -> try again with updated size
        storage.reset(new char[storageSize]);
      } else {
        return storage;
      }
    }
    throw std::runtime_error("failed to get local interface addresses");
  };

  auto const adaptersStorage = getAdaptersInfo();
  auto const adapters = reinterpret_cast<IP_ADAPTER_INFO const *>(adaptersStorage.get());

  auto fillPort = [](sockaddr *out, uint16_t port) {
    reinterpret_cast<sockaddr_in *>(out)->sin_port = htons(port);
  };

  auto fillHost = [](
      sockaddr *bcast,
      sockaddr const *ucast,
      sockaddr const *mask) {
    // broadcast = (unicast | ~mask) for IPv4
    reinterpret_cast<sockaddr_in *>(bcast)->sin_addr.S_un.S_addr =
        reinterpret_cast<sockaddr_in const *>(ucast)->sin_addr.S_un.S_addr
        | ~reinterpret_cast<sockaddr_in const *>(mask)->sin_addr.S_un.S_addr;
  };

  for(auto adapter = adapters; adapter != nullptr; adapter = adapter->Next) {
    for(auto ipAddress = &adapter->IpAddressList; ipAddress != nullptr; ipAddress = ipAddress->Next) {
      if(ipAddress->IpAddress.String == host) {
        const SockAddrInfo addr(ipAddress->IpAddress.String);
        const SockAddrInfo mask(ipAddress->IpMask.String);

        auto sas = std::make_shared<SockAddrStorage>();
        sas->Addr()->sa_family = AF_INET;
        fillPort(sas->Addr(), port);
        fillHost(sas->Addr(), addr.ForUdp().addr, mask.ForUdp().addr);
        *sas->AddrLen() = static_cast<socklen_t>(sizeof(sockaddr_in));

        return SocketAddress(std::move(sas));
      }
    }
  }

  throw std::runtime_error("failed to get broadcast address matching to \""
                           + to_string(*this) + "\"");
}

std::vector<SocketAddress>
SocketAddress::SocketAddressPriv::LocalAddresses()
{
  std::vector<SocketAddress> ret;

  // a special host name provides a list of local machine interface addresses
  const SockAddrInfo sockAddr("..localmachine");

  for(auto it = sockAddr.info.get(); it != nullptr; it = it->ai_next) {
    ret.emplace_back(std::make_shared<SockAddrStorage>(
                       it->ai_addr,
                       it->ai_addrlen));
  }

  return ret;
}

} // namespace sockpuppet

#endif // _WIN32
