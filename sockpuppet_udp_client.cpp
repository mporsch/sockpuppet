#include "socket.h" // for SocketUdp

#include <cstdlib> // for EXIT_SUCCESS
#include <iostream> // for std::cerr
#include <thread> // for std::this_thread::sleep_for

int main(int argc, char *argv[])
try {
  if(argc < 2) {
    std::cout << "Usage: " << argv[0]
      << " DESTINATION [SOURCE]\n\n"
         "\tDESTINATION is an address string to send to\n"
         "\tSOURCE is an address string to bind to, "
         "e.g. \"localhost:8554\""
      << std::endl;
  } else {
    SocketAddress const dstAddr(argv[1]);
    SocketAddress srcAddr;
    if(argc >= 3) {
      srcAddr = SocketAddress(argv[2]);
    }
    SocketUdp sock(srcAddr);

    std::cout << "sending from "
      << std::to_string(srcAddr) << " to "
      << std::to_string(dstAddr) << std::endl;

    for(;;) {
      std::this_thread::sleep_for(std::chrono::seconds(1));

      static char const hello[] = "hello";
      sock.SendTo(hello, sizeof(hello), dstAddr);
    }
  }

  return EXIT_SUCCESS;
} catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
