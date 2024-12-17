#include "sockpuppet/socket.h" // for SocketUdp

#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cout
#include <string_view> // for std::string_view

using namespace sockpuppet;

[[noreturn]] void Server(Address bindAddress)
{
  // bind a UDP socket to given address
  SocketUdp sock(bindAddress);

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
      << " SOURCE\n\n"
         "\tSOURCE is an address string to bind to, "
         "e.g. \"localhost:8554\""
      << std::endl;
    return EXIT_FAILURE;
  }

  // parse given address string
  Address bindAddress(argv[1]);

  // create and run a UDP socket
  Server(bindAddress);

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
