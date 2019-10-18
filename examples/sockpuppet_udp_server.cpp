#include "sockpuppet/socket.h" // for SocketUdp

#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cout

using namespace sockpuppet;

int main(int argc, char *argv[])
try {
  if(argc < 2) {
    std::cout << "Usage: " << argv[0]
      << " SOURCE\n\n"
         "\tSOURCE is an address string to bind to, "
         "e.g. \"localhost:8554\""
      << std::endl;
  } else {
    SocketAddress const srcAddr(argv[1]);
    SocketUdp sock(srcAddr);

    std::cout << "receiving at " << to_string(sock.LocalAddress())
              << std::endl;

    char buffer[256];
    for(;;) {
      auto const received = sock.Receive(buffer, sizeof(buffer));
      std::cout << std::string(buffer, received) << std::endl;
    }
  }

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
