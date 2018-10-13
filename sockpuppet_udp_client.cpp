#include "socket_address.h" // for SocketAddress
#include "socket.h" // for Socket

#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cerr
#include <thread> // for std::this_thread::sleep_for

int main(int argc, char *argv[])
try {
  if(argc < 2) {
    std::cout << "Usage: "
      << argv[0] << " "
      << "DESTINATION [SOURCE]\n\n"
      << "\tDESTINATION is an address string to transmit to\n"
         "\tSOURCE is an address string to bind to, "
         "e.g. \"localhost:8554\""
      << std::endl;
  } else {
    SocketAddress dst(argv[1]);
    SocketAddress src;
    if(argc >= 3) {
      src = SocketAddress(argv[2]);
    }
    SocketUdp sock(src);

    std::cout << "transmitting from "
      << std::to_string(src) << " to "
      << std::to_string(dst) << std::endl;

    for(;;) {
      static char const hello[] = "hello";

      std::this_thread::sleep_for(std::chrono::seconds(1));
      sock.Transmit(hello, sizeof(hello), dst);
    }
  }

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
