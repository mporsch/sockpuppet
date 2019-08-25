#include "socket_address.h" // for SocketAddress

#include <cstdlib> // for EXIT_SUCCESS
#include <iomanip> // for std::setw
#include <iostream> // for std::cerr
#include <string> // for std::string

using namespace sockpuppet;

int main(int, char **)
try {
  auto addresses = SocketAddress::GetLocalInterfaceAddresses();
  for(auto &&address : addresses)
    std::cout << std::setw(40)
              << to_string(address)
              << " <-- " << (address.IsV6()  ? "IPv6" : "IPv4")
              << std::endl;

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
