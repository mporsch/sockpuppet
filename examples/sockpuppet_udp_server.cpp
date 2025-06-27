#include "sockpuppet/socket.h" // for SocketUdp
#include "../src/address_impl.h" // for Address::impl
#include "../src/socket_impl.h" // for SocketUdp::impl

#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cout
#include <optional> // for std::optional
#include <string_view> // for std::string_view

using namespace sockpuppet;

[[noreturn]] void Server(Address bindAddress, std::optional<Address> remoteAddress)
{
  // bind a UDP socket to given address
  SocketUdp sock(bindAddress);

  if (remoteAddress) {
      // for IPv4 (IPv6 has its own ID and structure)
      auto opt = ip_mreq{
        reinterpret_cast<sockaddr_in const *>(remoteAddress->impl->ForUdp().addr)->sin_addr, // IP multicast address of group.
        reinterpret_cast<sockaddr_in const *>(bindAddress.impl->ForUdp().addr)->sin_addr // Local IP address of interface.
      };
      sock.impl->SetSockOpt(IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<char const *>(&opt), sizeof(opt));
  }

  // print the bound UDP socket address
  // (might have OS-assigned port number if
  // it has not been explicitly set in the bind address)
  std::cerr << "receiving at "
            << to_string(sock.LocalAddress())
            << std::endl;

  // receive and print until Ctrl-C
  for(;;) {
    // wait for and receive incoming data into provided buffer
    // negative timeout -> blocking until receipt
    char buffer[256];
    constexpr Duration noTimeout(-1);
    auto [receiveSize, fromAddr] = *sock.ReceiveFrom(
        buffer, sizeof(buffer),
        noTimeout);

    // print whatever has just been received
    if(receiveSize > 0U) {
      std::cout << std::string_view(buffer, receiveSize);
    } else {
      std::cerr << "<empty>";
    }
    std::cerr << " <- from " << to_string(fromAddr);
    std::cout << std::endl;
  }
}

int main(int argc, char *argv[])
try {
  if(argc < 2) {
    std::cerr << "Usage: " << argv[0]
      << " SOURCE [REMOTE]\n\n"
         "\tSOURCE is an address string to bind to, "
         "e.g. \"localhost:8554\""
         "\tREMOTE is a (multicast) address string to receive from, "
      << std::endl;
    return EXIT_FAILURE;
  }

  // parse given address string(s)
  Address bindAddress(argv[1]);
  auto remoteAddress = (argc >= 3 ? std::optional<Address>(argv[2]) : std::nullopt);

  // create and run a UDP socket
  Server(bindAddress, remoteAddress);

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
