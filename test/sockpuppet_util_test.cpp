#include "socket_address.h" // for SocketAddress
#include "socket.h" // for SocketUdp

#include <algorithm> // for std::transform
#include <cstdlib> // for EXIT_SUCCESS
#include <iomanip> // for std::setw
#include <iostream> // for std::cerr
#include <vector> // for std::vector

using namespace sockpuppet;

int main(int, char **)
try {
  std::vector<SocketUdp> sockets;
  auto const addresses = SocketAddress::LocalAddresses();
  std::transform(std::begin(addresses), std::end(addresses),
    std::back_inserter(sockets),
    [](SocketAddress const &address) -> SocketUdp {
      std::cout << std::setw(40)
                << to_string(address)
                << " <-- " << (address.IsV6()  ? "IPv6" : "IPv4")
                << std::endl;

      return SocketUdp(address);
    });

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
