#include "socket_address.h" // for SocketAddress
#include "socket.h" // for Socket

#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cerr

int main(int argc, char *argv[])
try {
  SocketAddress address;
  std::cout << std::to_string(address) << std::endl;

  Socket socket(address);

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
