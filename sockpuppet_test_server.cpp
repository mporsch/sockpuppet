#include "socket_address.h" // for SocketAddress
#include "socket.h" // for Socket

#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cout

int main(int argc, char *argv[])
try {
  if(argc < 2) {
    std::cout << "Usage: "
      << argv[0] << " "
      << "URI\n\n"
      << "\t where URI is an address string to bind to, e.g. \"localhost:8554\""
      << std::endl;
  } else {
    SocketAddress address(argv[1]);
    std::cout << std::to_string(address) << std::endl;

    Socket socket(address);
  }

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
